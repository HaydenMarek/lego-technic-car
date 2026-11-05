// Wi-Fi and OTA update. See ota.h and ota_logic.h (protocol).
// Based on pico-examples pico_w/wifi/ota_update (BSD-3-Clause), changed to:
// - run next to Bluetooth on the BTstack run loop, never blocking it;
// - stop the car before writing flash (the controller stays connected:
//   an Xbox controller turns off when the host disconnects it);
// - write flash with interrupts off (flash_safe_execute) and check each page;
// - check token, UF2 blocks and addresses (ota_logic.c).

#include "ota.h"

#include <stdio.h>
#include <string.h>

#include <pico.h> // first: the boot/ and hardware/ headers need it

#include <boot/picobin.h>
#include <boot/picoboot_constants.h> // REBOOT2_FLAG_*
#include <btstack_run_loop.h>
#include <hardware/flash.h>
#include <hardware/regs/addressmap.h>
#include <hardware/watchdog.h>
#include <lwip/apps/mdns.h>
#include <lwip/pbuf.h>
#include <lwip/tcp.h>
#include <pico/bootrom.h>
#include <pico/cyw43_arch.h>
#include <pico/flash.h>
#include <pico/time.h>

#include "controller.h"
#include "ota_logic.h"
#include "ota_secrets.h" // generated from firmware/.env by CMake (build dir only)

#define TICK_MS 100
#define WIFI_JOIN_TIMEOUT_MS 30000
#define WIFI_RETRY_MS 15000
#define STOP_SETTLE_MS 100 // drive loop (20 ms) has coasted the motors by then
#define CLIENT_IDLE_MS 10000    // upload stalls this long: cancel
#define REBOOT_AFTER_DONE_MS 300 // let "DONE" reach the PC first

// Written to watchdog scratch[0] just before the OTA reboot. picotool's
// "load -x" ALSO reboots with type FLASH_UPDATE (seen 2026-11-04), so the boot
// type alone cannot tell OTA from USB. Scratch 0..3 survive a reboot (not a
// power cut); the boot ROM uses only 4..7.
#define OTA_REBOOT_MAGIC 0x4f544131u // "OTA1"
#define SCAN_MAX_SEEN 16

typedef enum { WIFI_JOINING, WIFI_UP, WIFI_WAIT } wifi_state_t;

typedef enum {
    UP_IDLE,     // listening
    UP_HEADER,   // PC connected, waiting for the header
    UP_STOPPING, // header OK, letting the car stop
    UP_DATA,     // receiving chunks and writing flash
    UP_REBOOT,   // all written, rebooting soon
} up_state_t;

static btstack_timer_source_t tick_timer;

static wifi_state_t wifi_state;
static uint32_t wifi_since_ms;
static bool mdns_added;

static struct tcp_pcb *listen_pcb;
static struct tcp_pcb *client;
static up_state_t up_state;
static uint32_t up_since_ms;   // when up_state last changed
static uint32_t last_rx_ms;
static bool car_stopped;       // controller paused, driving blocked
static ota_rx_t rx;
static uint8_t buf[OTA_CHUNK_SIZE] __attribute__((aligned(4)));
static size_t buf_len;

static int boot_partition = -1;
static bool buy_pending;
static bool via_update;

// Work area for boot ROM calls (partition lookup, buy).
static uint8_t rom_workarea[4096] __attribute__((aligned(4)));

static uint32_t now_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}

static struct netif *sta_netif(void) {
    return &cyw43_state.netif[CYW43_ITF_STA];
}

bool ota_active(void) {
    return car_stopped;
}

//
// Wi-Fi
//
static void wifi_join(void) {
    printf("wifi: joining \"%s\"\n", WIFI_SSID);
    // Async: returns at once; we poll the link status in the tick.
    int rc = cyw43_arch_wifi_connect_async(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK);
    wifi_state = rc == 0 ? WIFI_JOINING : WIFI_WAIT;
    wifi_since_ms = now_ms();
    if (rc != 0) {
        printf("wifi: join failed to start (%d)\n", rc);
    }
}

// Network not found: list what the Pico can see (helps with 5 GHz-only
// routers or a typo in WIFI_SSID). Channels 1..13 = 2.4 GHz.
static char scan_seen[SCAN_MAX_SEEN][33];
static int scan_seen_count;

static int on_scan_result(void *env, const cyw43_ev_scan_result_t *r) {
    (void)env;
    if (r == NULL || r->ssid_len == 0) {
        return 0;
    }
    char ssid[33];
    memcpy(ssid, r->ssid, r->ssid_len);
    ssid[r->ssid_len] = 0;
    for (int i = 0; i < scan_seen_count; i++) {
        if (strcmp(scan_seen[i], ssid) == 0) {
            return 0; // one line per name
        }
    }
    if (scan_seen_count < SCAN_MAX_SEEN) {
        strcpy(scan_seen[scan_seen_count++], ssid);
        printf("wifi: sees \"%s\" channel %u, %d dBm\n", ssid, r->channel, r->rssi);
    }
    return 0;
}

static void wifi_scan(void) {
    cyw43_wifi_scan_options_t opts = {0};
    scan_seen_count = 0;
    if (cyw43_wifi_scan(&cyw43_state, &opts, NULL, on_scan_result) == 0) {
        printf("wifi: scanning for networks...\n");
    }
}

static const char *link_name(int status) {
    switch (status) {
    case CYW43_LINK_FAIL: return "failed";
    case CYW43_LINK_NONET: return "network not found";
    case CYW43_LINK_BADAUTH: return "wrong password";
    default: return "timeout";
    }
}

static void wifi_tick(void) {
    int status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
    uint32_t elapsed = now_ms() - wifi_since_ms;
    static uint32_t last_status_ms;
    if (wifi_state != WIFI_UP && now_ms() - last_status_ms >= 10000) {
        last_status_ms = now_ms();
        printf("wifi: state %d, link %d, %lu s\n", wifi_state, status,
               (unsigned long)(elapsed / 1000));
    }
    switch (wifi_state) {
    case WIFI_JOINING:
        if (status == CYW43_LINK_UP) {
            wifi_state = WIFI_UP;
            if (!mdns_added) {
                mdns_resp_add_netif(sta_netif(), CYW43_HOST_NAME);
                mdns_added = true;
            }
            printf("wifi: connected, %s (%s.local), OTA port %d\n",
                   ip4addr_ntoa(netif_ip4_addr(sta_netif())), CYW43_HOST_NAME, OTA_PORT);
        } else if (status < 0 || elapsed >= WIFI_JOIN_TIMEOUT_MS) {
            printf("wifi: %s, retry in %d s\n", link_name(status), WIFI_RETRY_MS / 1000);
            cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
            wifi_state = WIFI_WAIT;
            wifi_since_ms = now_ms();
            if (status == CYW43_LINK_NONET) {
                wifi_scan();
            }
        }
        break;
    case WIFI_UP:
        if (status != CYW43_LINK_UP) {
            printf("wifi: lost connection, retry in %d s\n", WIFI_RETRY_MS / 1000);
            cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
            wifi_state = WIFI_WAIT;
            wifi_since_ms = now_ms();
        }
        break;
    case WIFI_WAIT:
        if (elapsed >= WIFI_RETRY_MS && !cyw43_wifi_scan_active(&cyw43_state)) {
            wifi_join();
        }
        break;
    }
}

//
// Upload over TCP
//
static void set_state(up_state_t s) {
    up_state = s;
    up_since_ms = now_ms();
}

static void send_reply(const char *reply) {
    if (client) {
        tcp_write(client, reply, OTA_REPLY_SIZE, TCP_WRITE_FLAG_COPY);
        tcp_output(client);
    }
}

// Returns ERR_ABRT if the pcb was aborted (lwIP callbacks must return that).
static err_t close_client(void) {
    err_t err = ERR_OK;
    if (client) {
        tcp_arg(client, NULL);
        tcp_recv(client, NULL);
        tcp_err(client, NULL);
        tcp_poll(client, NULL, 0);
        if (tcp_close(client) != ERR_OK) {
            tcp_abort(client);
            err = ERR_ABRT;
        }
        client = NULL;
    }
    return err;
}

// Back to normal driving: let the controller reconnect.
static void resume_car(void) {
    if (car_stopped) {
        car_stopped = false;
        controller_resume();
        printf("ota: cancelled, controller may reconnect\n");
    }
}

static err_t fail(ota_status_t s) {
    printf("ota: ERROR %s (%.4s)\n", ota_status_name(s), ota_status_reply(s));
    send_reply(ota_status_reply(s));
    err_t err = close_client();
    resume_car();
    set_state(UP_IDLE);
    return err;
}

// Find the free slot (the A/B partition we are NOT running from).
static ota_status_t find_slot(uint32_t family, uint32_t *start, uint32_t *size) {
    if (boot_partition < 0) {
        return OTA_ERR_PARTITION; // no partition table: flash it over USB first
    }
    resident_partition_t part;
    rom_flash_flush_cache();
    int rc = rom_get_uf2_target_partition(rom_workarea, sizeof rom_workarea, family, &part);
    if (rc < 0 || rc == boot_partition) {
        printf("ota: target partition %d (running from %d)\n", rc, boot_partition);
        return OTA_ERR_PARTITION;
    }
    uint32_t first = (part.permissions_and_location & PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_BITS) >>
                     PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_LSB;
    uint32_t last = (part.permissions_and_location & PICOBIN_PARTITION_LOCATION_LAST_SECTOR_BITS) >>
                    PICOBIN_PARTITION_LOCATION_LAST_SECTOR_LSB;
    *start = first * OTA_FLASH_SECTOR;
    *size = (last + 1 - first) * OTA_FLASH_SECTOR;
    printf("ota: writing partition %d at 0x%06lx, %lu KB\n", rc, (unsigned long)*start,
           (unsigned long)(*size / 1024));
    return OTA_OK;
}

static err_t on_header(void) {
    ota_header_t h;
    ota_status_t s = ota_header_parse(buf, OTA_TOKEN, &h);
    uint32_t start = 0, size = 0;
    if (s == OTA_OK) {
        s = find_slot(h.family, &start, &size);
    }
    if (s == OTA_OK) {
        s = ota_rx_init(&rx, &h, start, size);
    }
    if (s != OTA_OK) {
        return fail(s);
    }
    printf("ota: update %lu KB: stopping car\n",
           (unsigned long)(h.num_blocks / 4));
    car_stopped = true; // main.c: drive gets no input -> disarm, coast
    controller_pause();
    set_state(UP_STOPPING); // tick sends "RDY!" after STOP_SETTLE_MS
    return ERR_OK;
}

typedef struct {
    const ota_write_t *w;
    bool ok;
} flash_job_t;

// Runs with interrupts off (flash_safe_execute): no flash code may run while
// flash is busy, and our IRQ handlers live in flash.
static void flash_job(void *param) {
    flash_job_t *job = param;
    const ota_write_t *w = job->w;
    if (w->erase) {
        flash_range_erase(w->erase_offset, OTA_FLASH_SECTOR);
    }
    flash_range_program(w->offset, w->data, OTA_FLASH_PAGE);
    // Read back through the untranslated, uncached window.
    job->ok = memcmp((const void *)(XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE + w->offset), w->data,
                     OTA_FLASH_PAGE) == 0;
}

static err_t on_chunk(void) {
    for (size_t i = 0; i < OTA_CHUNK_SIZE / OTA_UF2_BLOCK_SIZE && !ota_rx_done(&rx); i++) {
        ota_write_t w;
        ota_status_t s = ota_rx_block(&rx, buf + i * OTA_UF2_BLOCK_SIZE, &w);
        if (s != OTA_OK) {
            return fail(s);
        }
        flash_job_t job = {.w = &w, .ok = false};
        if (flash_safe_execute(flash_job, &job, 100) != PICO_OK || !job.ok) {
            return fail(OTA_ERR_FLASH);
        }
    }
    buf_len = 0;
    if (!ota_rx_done(&rx)) {
        send_reply("ACK!");
        return ERR_OK;
    }
    printf("ota: all %lu blocks written, rebooting into the new image\n",
           (unsigned long)rx.num_blocks);
    send_reply("DONE");
    set_state(UP_REBOOT);
    return close_client();
}

static err_t on_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    (void)arg;
    (void)err;
    if (p == NULL) { // PC closed the connection
        client = pcb;
        close_client();
        if (up_state != UP_IDLE && up_state != UP_REBOOT) {
            printf("ota: PC disconnected early\n");
            resume_car();
            set_state(UP_IDLE);
        }
        return ERR_OK;
    }
    last_rx_ms = now_ms();
    size_t need = up_state == UP_HEADER ? OTA_HEADER_SIZE
                  : up_state == UP_DATA ? OTA_CHUNK_SIZE
                                        : 0;
    if (buf_len + p->tot_len > need) {
        // The PC must wait for our reply before sending more.
        tcp_recved(pcb, p->tot_len);
        pbuf_free(p);
        return fail(OTA_ERR_PROTOCOL);
    }
    buf_len += pbuf_copy_partial(p, buf + buf_len, p->tot_len, 0);
    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);

    if (buf_len < need) {
        return ERR_OK;
    }
    return up_state == UP_HEADER ? on_header() : on_chunk();
}

static void on_tcp_err(void *arg, err_t err) {
    (void)arg;
    // lwIP already freed the pcb.
    client = NULL;
    if (up_state != UP_IDLE && up_state != UP_REBOOT) {
        printf("ota: connection error %d\n", err);
        resume_car();
        set_state(UP_IDLE);
    }
}

static err_t on_accept(void *arg, struct tcp_pcb *pcb, err_t err) {
    (void)arg;
    if (err != ERR_OK || pcb == NULL) {
        return ERR_VAL;
    }
    if (client != NULL || up_state != UP_IDLE) {
        tcp_abort(pcb); // one upload at a time
        return ERR_ABRT;
    }
    client = pcb;
    buf_len = 0;
    last_rx_ms = now_ms();
    set_state(UP_HEADER);
    tcp_recv(pcb, on_recv);
    tcp_err(pcb, on_tcp_err);
    printf("ota: PC connected\n");
    return ERR_OK;
}

static void start_server(void) {
    struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (pcb == NULL || tcp_bind(pcb, IP_ANY_TYPE, OTA_PORT) != ERR_OK) {
        printf("ota: cannot open port %d\n", OTA_PORT);
        return;
    }
    listen_pcb = tcp_listen_with_backlog(pcb, 1);
    if (listen_pcb == NULL) {
        tcp_close(pcb);
        printf("ota: listen failed\n");
        return;
    }
    tcp_accept(listen_pcb, on_accept);
}

static void reboot_into_update(void) {
    printf("ota: rebooting\n");
    sleep_ms(50); // let USB serial send the line
    watchdog_hw->scratch[0] = OTA_REBOOT_MAGIC; // new image: "you came from OTA"
    // FLASH_UPDATE: boot ROM tries the image at this address first (TBYB).
    rom_reboot(REBOOT2_FLAG_REBOOT_TYPE_FLASH_UPDATE | REBOOT2_FLAG_NO_RETURN_ON_SUCCESS, 10,
               XIP_BASE + rx.slot_start, 0);
    while (true) {
        tight_loop_contents();
    }
}

static void upload_tick(void) {
    uint32_t now = now_ms();
    switch (up_state) {
    case UP_STOPPING:
        if (now - up_since_ms >= STOP_SETTLE_MS) {
            printf("ota: car stopped, receiving\n");
            buf_len = 0;
            last_rx_ms = now;
            set_state(UP_DATA);
            send_reply("RDY!");
        }
        break;
    case UP_HEADER:
    case UP_DATA:
        if (now - last_rx_ms >= CLIENT_IDLE_MS) {
            printf("ota: upload stalled\n");
            fail(OTA_ERR_PROTOCOL);
        }
        break;
    case UP_REBOOT:
        if (now - up_since_ms >= REBOOT_AFTER_DONE_MS) {
            reboot_into_update();
        }
        break;
    case UP_IDLE:
        break;
    }
}

//
// Try before you buy
//
static void buy_tick(void) {
    ota_buy_action_t a = ota_buy_decide(buy_pending, via_update, controller_bt_ready(),
                                        wifi_state == WIFI_UP, now_ms());
    if (a == OTA_BUY_NOW) {
        printf("ota: buying...\n");
        // The SDK's rom_explicit_buy() already wraps the ROM call in
        // flash_safe_execute(). Do NOT wrap it again: flash_safe_execute keeps
        // one saved IRQ state per core, so nesting leaves interrupts OFF for
        // good (all timers, Bluetooth, USB stop). Seen on the Pico 2026-11-04.
        int rc = rom_explicit_buy(rom_workarea, sizeof rom_workarea);
        // rom_explicit_buy() switches the watchdog OFF (it stops the boot ROM's
        // TBYB timer). That is our 1 s failsafe watchdog too: turn it back on.
        // The load value is unchanged, so this restores main.c's setting.
        watchdog_update();
        hw_set_bits(&watchdog_hw->ctrl, WATCHDOG_CTRL_ENABLE_BITS);
        buy_pending = false;
        printf("ota: new image bought%s, watchdog %s\n", rc == 0 ? "" : " (ERROR)",
               (watchdog_hw->ctrl & WATCHDOG_CTRL_ENABLE_BITS) ? "on" : "OFF");
    } else if (a == OTA_BUY_REVERT) {
        printf("ota: new image not healthy in %d s, back to the old one\n",
               OTA_BUY_DEADLINE_MS / 1000);
        sleep_ms(50);
        watchdog_reboot(0, 0, 10); // normal reboot: boot ROM skips the unbought image
        while (true) {
            tight_loop_contents();
        }
    }
}

static void on_tick(btstack_timer_source_t *ts) {
    wifi_tick();
    upload_tick();
    buy_tick();
    btstack_run_loop_set_timer(ts, TICK_MS);
    btstack_run_loop_add_timer(ts);
}

void ota_init(void) {
    boot_info_t info;
    if (rom_get_boot_info(&info)) {
        boot_partition = info.partition;
        buy_pending = info.tbyb_and_update_info & BOOT_TBYB_AND_UPDATE_FLAG_BUY_PENDING;
    }
    via_update = rom_get_last_boot_type() == BOOT_TYPE_FLASH_UPDATE &&
                 watchdog_hw->scratch[0] == OTA_REBOOT_MAGIC;
    watchdog_hw->scratch[0] = 0; // only for this one boot
    printf("boot: partition %d%s%s\n", boot_partition, via_update ? ", after OTA update" : "",
           buy_pending ? ", not bought yet" : "");
    if (boot_partition < 0) {
        printf("boot: WARNING no partition table, OTA off. See firmware/README.md\n");
    }

    cyw43_arch_enable_sta_mode();
    cyw43_arch_lwip_begin();
    mdns_resp_init();
    start_server();
    cyw43_arch_lwip_end();
    wifi_join();

    btstack_run_loop_set_timer_handler(&tick_timer, on_tick);
    btstack_run_loop_set_timer(&tick_timer, TICK_MS);
    btstack_run_loop_add_timer(&tick_timer);
}
