#include "ota_logic.h"

#include <string.h>

#define UF2_MAGIC_START0 0x0A324655u
#define UF2_MAGIC_START1 0x9E5D5157u
#define UF2_MAGIC_END 0x0AB16F30u
#define UF2_FLAG_NOT_MAIN_FLASH 0x00000001u
#define UF2_FLAG_FAMILY_ID_PRESENT 0x00002000u

static const char header_magic[8] = {'L', 'E', 'G', 'O', 'O', 'T', 'A', '1'};

// Read little-endian u32 from any alignment.
static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static const struct {
    const char *reply;
    const char *name;
} statuses[] = {
    [OTA_OK] = {"ACK!", "ok"},
    [OTA_ERR_MAGIC] = {"EMAG", "bad header"},
    [OTA_ERR_TOKEN] = {"ETOK", "wrong token"},
    [OTA_ERR_FAMILY] = {"EFAM", "not an RP2350 Arm image"},
    [OTA_ERR_SIZE] = {"ESIZ", "image size"},
    [OTA_ERR_UF2] = {"EUF2", "bad UF2 block"},
    [OTA_ERR_ORDER] = {"EORD", "block order"},
    [OTA_ERR_ADDR] = {"EADR", "block address"},
    [OTA_ERR_PARTITION] = {"EPAR", "no partition table / free slot"},
    [OTA_ERR_FLASH] = {"EFLS", "flash write failed"},
    [OTA_ERR_BUSY] = {"EBSY", "busy"},
    [OTA_ERR_PROTOCOL] = {"EPRO", "protocol error"},
};

const char *ota_status_reply(ota_status_t s) {
    return (unsigned)s < sizeof statuses / sizeof statuses[0] ? statuses[s].reply : "E???";
}

const char *ota_status_name(ota_status_t s) {
    return (unsigned)s < sizeof statuses / sizeof statuses[0] ? statuses[s].name : "unknown";
}

ota_status_t ota_header_parse(const uint8_t *buf, const char *token, ota_header_t *out) {
    if (memcmp(buf, header_magic, sizeof header_magic) != 0) {
        return OTA_ERR_MAGIC;
    }
    // Token field: 48 bytes, must be exactly the token followed by zeros.
    const uint8_t *field = buf + 16;
    size_t len = strlen(token);
    if (len == 0 || len > OTA_TOKEN_MAX || memcmp(field, token, len) != 0) {
        return OTA_ERR_TOKEN;
    }
    for (size_t i = len; i < OTA_HEADER_SIZE - 16; i++) {
        if (field[i] != 0) {
            return OTA_ERR_TOKEN;
        }
    }
    out->num_blocks = le32(buf + 8);
    out->family = le32(buf + 12);
    if (out->family != OTA_FAMILY_RP2350_ARM_S) {
        return OTA_ERR_FAMILY;
    }
    if (out->num_blocks == 0) {
        return OTA_ERR_SIZE;
    }
    return OTA_OK;
}

ota_status_t ota_rx_init(ota_rx_t *rx, const ota_header_t *h, uint32_t slot_start,
                         uint32_t slot_size) {
    memset(rx, 0, sizeof *rx);
    // Each block carries one 256-byte page, so the image needs at least this much.
    if ((uint64_t)h->num_blocks * OTA_FLASH_PAGE > slot_size) {
        return OTA_ERR_SIZE;
    }
    rx->slot_start = slot_start;
    rx->slot_size = slot_size;
    rx->family = h->family;
    rx->num_blocks = h->num_blocks;
    rx->next_addr = OTA_XIP_BASE;
    rx->erased_end = slot_start;
    return OTA_OK;
}

ota_status_t ota_rx_block(ota_rx_t *rx, const uint8_t *block, ota_write_t *w) {
    if (ota_rx_done(rx)) {
        return OTA_ERR_PROTOCOL;
    }
    uint32_t flags = le32(block + 8);
    uint32_t target = le32(block + 12);
    uint32_t payload = le32(block + 16);
    uint32_t block_no = le32(block + 20);
    uint32_t num_blocks = le32(block + 24);
    uint32_t family = le32(block + 28);

    if (le32(block) != UF2_MAGIC_START0 || le32(block + 4) != UF2_MAGIC_START1 ||
        le32(block + OTA_UF2_BLOCK_SIZE - 4) != UF2_MAGIC_END) {
        return OTA_ERR_UF2;
    }
    if ((flags & UF2_FLAG_NOT_MAIN_FLASH) || !(flags & UF2_FLAG_FAMILY_ID_PRESENT) ||
        payload != OTA_FLASH_PAGE) {
        return OTA_ERR_UF2;
    }
    if (family != rx->family) {
        return OTA_ERR_FAMILY;
    }
    if (block_no != rx->next_block || num_blocks != rx->num_blocks) {
        return OTA_ERR_ORDER;
    }
    // The image is linked at XIP_BASE; the boot ROM maps the slot there.
    if (target < rx->next_addr || target % OTA_FLASH_PAGE != 0 ||
        target - OTA_XIP_BASE > rx->slot_size - OTA_FLASH_PAGE) {
        return OTA_ERR_ADDR;
    }

    uint32_t offset = rx->slot_start + (target - OTA_XIP_BASE);
    w->erase = offset >= rx->erased_end;
    if (w->erase) {
        w->erase_offset = offset & ~(OTA_FLASH_SECTOR - 1);
        rx->erased_end = w->erase_offset + OTA_FLASH_SECTOR;
    }
    w->offset = offset;
    w->data = block + 32;

    rx->next_block++;
    rx->next_addr = target + OTA_FLASH_PAGE;
    return OTA_OK;
}

bool ota_rx_done(const ota_rx_t *rx) {
    return rx->num_blocks > 0 && rx->next_block >= rx->num_blocks;
}

ota_buy_action_t ota_buy_decide(bool buy_pending, bool via_update, bool bt_ready, bool wifi_up,
                                uint32_t ms_since_boot) {
    if (!buy_pending) {
        return OTA_BUY_NOTHING;
    }
    if (!via_update) {
        return bt_ready ? OTA_BUY_NOW : OTA_BUY_WAIT;
    }
    // After OTA: Wi-Fi must work too, or we could not update again.
    if (bt_ready && wifi_up) {
        return OTA_BUY_NOW;
    }
    return ms_since_boot >= OTA_BUY_DEADLINE_MS ? OTA_BUY_REVERT : OTA_BUY_WAIT;
}
