// Over-the-air (OTA) update: the parts that need no Pico SDK.
// Upload protocol, UF2 block checks, flash addresses, and when to "buy"
// a new image. Tested on the PC (tests/test_ota_logic.c).
//
// Protocol (TCP port OTA_PORT, see tools/ota.mjs):
//   PC -> Pico  header, OTA_HEADER_SIZE bytes:
//                 0..7   "LEGOOTA1"
//                 8..11  number of UF2 blocks (u32, little endian)
//                 12..15 UF2 family ID (u32, little endian)
//                 16..63 token, zero padded
//   Pico -> PC  4-byte reply: "RDY!" or an error code
//   PC -> Pico  chunks of OTA_CHUNK_SIZE bytes (4 UF2 blocks, last one zero padded)
//   Pico -> PC  4-byte reply after each chunk: "ACK!", "DONE" (last) or an error
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define OTA_PORT 4242
#define OTA_HEADER_SIZE 64
#define OTA_TOKEN_MAX 47 // chars; header has 48 bytes incl. the zero
#define OTA_UF2_BLOCK_SIZE 512
#define OTA_CHUNK_SIZE (4 * OTA_UF2_BLOCK_SIZE)
#define OTA_REPLY_SIZE 4

#define OTA_FLASH_SECTOR 4096u
#define OTA_FLASH_PAGE 256u
#define OTA_XIP_BASE 0x10000000u
#define OTA_FAMILY_RP2350_ARM_S 0xe48bff59u

typedef enum {
    OTA_OK = 0,
    OTA_ERR_MAGIC,       // header does not start with "LEGOOTA1"
    OTA_ERR_TOKEN,       // wrong token
    OTA_ERR_FAMILY,      // not an RP2350 Arm (secure) image
    OTA_ERR_SIZE,        // zero blocks, or image bigger than the slot
    OTA_ERR_UF2,         // bad UF2 block (magic, flags, payload size)
    OTA_ERR_ORDER,       // blocks out of order or wrong count
    OTA_ERR_ADDR,        // target address outside the slot or not ascending
    OTA_ERR_PARTITION,   // no free slot: partition table missing
    OTA_ERR_FLASH,       // flash erase/program failed
    OTA_ERR_BUSY,        // another upload is running
    OTA_ERR_PROTOCOL,    // too much data, or data at the wrong time
} ota_status_t;

// 4-char reply for the PC, e.g. "ETOK". Not zero terminated.
const char *ota_status_reply(ota_status_t s);
const char *ota_status_name(ota_status_t s);

typedef struct {
    uint32_t num_blocks;
    uint32_t family;
} ota_header_t;

// buf has OTA_HEADER_SIZE bytes. token = expected token (zero terminated).
ota_status_t ota_header_parse(const uint8_t *buf, const char *token, ota_header_t *out);

// Receiver state for one upload.
typedef struct {
    uint32_t slot_start; // physical flash offset of the slot being written
    uint32_t slot_size;
    uint32_t family;
    uint32_t num_blocks;
    uint32_t next_block; // block number we expect next
    uint32_t next_addr;  // lowest allowed target address (blocks must ascend)
    uint32_t erased_end; // physical offset: sectors below this are erased
} ota_rx_t;

// Checks the header fits the slot. Call once the slot is known.
ota_status_t ota_rx_init(ota_rx_t *rx, const ota_header_t *h, uint32_t slot_start,
                         uint32_t slot_size);

// What to do with one UF2 block.
typedef struct {
    bool erase;            // erase the sector at erase_offset first
    uint32_t erase_offset; // physical flash offset, sector aligned
    uint32_t offset;       // physical flash offset to program, page aligned
    const uint8_t *data;   // OTA_FLASH_PAGE bytes, points into the block
} ota_write_t;

// Checks one UF2 block (OTA_UF2_BLOCK_SIZE bytes) and says where it goes.
// On OTA_OK, do the write, then the block is counted.
ota_status_t ota_rx_block(ota_rx_t *rx, const uint8_t *block, ota_write_t *w);

bool ota_rx_done(const ota_rx_t *rx);

// "Try before you buy" (TBYB): a new image must be bought, or the boot ROM
// goes back to the old one on the next reboot.
typedef enum {
    OTA_BUY_NOTHING, // already bought, nothing pending
    OTA_BUY_WAIT,    // not healthy yet, keep waiting
    OTA_BUY_NOW,     // healthy: buy
    OTA_BUY_REVERT,  // OTA image not healthy in time: reboot to the old slot
} ota_buy_action_t;

// The new image must be healthy within this time after boot.
// Below the boot ROM's own TBYB limit (16.7 s).
#define OTA_BUY_DEADLINE_MS 15000

// buy_pending: boot ROM says this image is not bought yet.
// via_update:  booted by an OTA flash-update reboot (an old slot exists).
// After a USB flash there is no old slot, so Wi-Fi is not needed and we
// never revert (that would be a reboot loop).
ota_buy_action_t ota_buy_decide(bool buy_pending, bool via_update, bool bt_ready, bool wifi_up,
                                uint32_t ms_since_boot);
