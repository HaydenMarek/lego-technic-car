// Tests for ota_logic.c. Exit code 0 = all passed.
#include <stdio.h>
#include <string.h>

#include "ota_logic.h"

static int failures;

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
            failures++;                                                \
        }                                                              \
    } while (0)

#define CHECK_EQ(a, b)                                                          \
    do {                                                                        \
        long a_ = (long)(a), b_ = (long)(b);                                    \
        if (a_ != b_) {                                                         \
            printf("FAIL %s:%d: %s == %ld, want %ld\n", __FILE__, __LINE__, #a, a_, b_); \
            failures++;                                                         \
        }                                                                       \
    } while (0)

#define TOKEN "secret-token"
#define SLOT_START 0x2000u
#define SLOT_SIZE (2000u * 1024u)

static void put32(uint8_t *p, uint32_t v) {
    p[0] = v;
    p[1] = v >> 8;
    p[2] = v >> 16;
    p[3] = v >> 24;
}

static void make_header(uint8_t *h, uint32_t blocks, uint32_t family, const char *token) {
    memset(h, 0, OTA_HEADER_SIZE);
    memcpy(h, "LEGOOTA1", 8);
    put32(h + 8, blocks);
    put32(h + 12, family);
    memcpy(h + 16, token, strlen(token));
}

static void make_block(uint8_t *b, uint32_t no, uint32_t count, uint32_t target) {
    memset(b, 0, OTA_UF2_BLOCK_SIZE);
    put32(b, 0x0A324655u);
    put32(b + 4, 0x9E5D5157u);
    put32(b + 8, 0x00002000u); // family ID present
    put32(b + 12, target);
    put32(b + 16, 256);
    put32(b + 20, no);
    put32(b + 24, count);
    put32(b + 28, OTA_FAMILY_RP2350_ARM_S);
    memset(b + 32, (int)(no & 0xff), 256);
    put32(b + OTA_UF2_BLOCK_SIZE - 4, 0x0AB16F30u);
}

static void test_header(void) {
    uint8_t h[OTA_HEADER_SIZE];
    ota_header_t out;

    make_header(h, 10, OTA_FAMILY_RP2350_ARM_S, TOKEN);
    CHECK_EQ(ota_header_parse(h, TOKEN, &out), OTA_OK);
    CHECK_EQ(out.num_blocks, 10);
    CHECK_EQ(out.family, OTA_FAMILY_RP2350_ARM_S);

    make_header(h, 10, OTA_FAMILY_RP2350_ARM_S, "wrong");
    CHECK_EQ(ota_header_parse(h, TOKEN, &out), OTA_ERR_TOKEN);

    // Token prefix is not enough: "secret-token-x" must not match "secret-token".
    make_header(h, 10, OTA_FAMILY_RP2350_ARM_S, TOKEN "-x");
    CHECK_EQ(ota_header_parse(h, TOKEN, &out), OTA_ERR_TOKEN);

    // Empty expected token never matches (build without a token).
    make_header(h, 10, OTA_FAMILY_RP2350_ARM_S, "");
    CHECK_EQ(ota_header_parse(h, "", &out), OTA_ERR_TOKEN);

    make_header(h, 10, 0xe48bff57u /* absolute */, TOKEN);
    CHECK_EQ(ota_header_parse(h, TOKEN, &out), OTA_ERR_FAMILY);

    make_header(h, 0, OTA_FAMILY_RP2350_ARM_S, TOKEN);
    CHECK_EQ(ota_header_parse(h, TOKEN, &out), OTA_ERR_SIZE);

    make_header(h, 10, OTA_FAMILY_RP2350_ARM_S, TOKEN);
    h[0] = 'X';
    CHECK_EQ(ota_header_parse(h, TOKEN, &out), OTA_ERR_MAGIC);
}

static ota_rx_t start(uint32_t blocks) {
    ota_header_t h = {.num_blocks = blocks, .family = OTA_FAMILY_RP2350_ARM_S};
    ota_rx_t rx;
    CHECK_EQ(ota_rx_init(&rx, &h, SLOT_START, SLOT_SIZE), OTA_OK);
    return rx;
}

static void test_too_big(void) {
    ota_header_t h = {.num_blocks = SLOT_SIZE / 256 + 1, .family = OTA_FAMILY_RP2350_ARM_S};
    ota_rx_t rx;
    CHECK_EQ(ota_rx_init(&rx, &h, SLOT_START, SLOT_SIZE), OTA_ERR_SIZE);
}

// 20 pages = 5120 bytes: erase sector 0 at block 0, sector 1 at block 16.
static void test_erase_and_offsets(void) {
    ota_rx_t rx = start(20);
    uint8_t b[OTA_UF2_BLOCK_SIZE];
    ota_write_t w;
    for (uint32_t i = 0; i < 20; i++) {
        make_block(b, i, 20, OTA_XIP_BASE + i * 256);
        CHECK_EQ(ota_rx_block(&rx, b, &w), OTA_OK);
        CHECK_EQ(w.offset, SLOT_START + i * 256);
        CHECK(w.data == b + 32);
        if (i == 0) {
            CHECK(w.erase);
            CHECK_EQ(w.erase_offset, SLOT_START);
        } else if (i == 16) {
            CHECK(w.erase);
            CHECK_EQ(w.erase_offset, SLOT_START + 4096);
        } else {
            CHECK(!w.erase);
        }
    }
    CHECK(ota_rx_done(&rx));
    // Extra blocks after the last are an error, not written.
    make_block(b, 20, 20, OTA_XIP_BASE + 20 * 256);
    CHECK_EQ(ota_rx_block(&rx, b, &w), OTA_ERR_PROTOCOL);
}

// A gap in addresses erases the sector where the next block lands.
static void test_gap(void) {
    ota_rx_t rx = start(2);
    uint8_t b[OTA_UF2_BLOCK_SIZE];
    ota_write_t w;
    make_block(b, 0, 2, OTA_XIP_BASE);
    CHECK_EQ(ota_rx_block(&rx, b, &w), OTA_OK);
    make_block(b, 1, 2, OTA_XIP_BASE + 3 * 4096 + 512);
    CHECK_EQ(ota_rx_block(&rx, b, &w), OTA_OK);
    CHECK(w.erase);
    CHECK_EQ(w.erase_offset, SLOT_START + 3 * 4096);
    CHECK_EQ(w.offset, SLOT_START + 3 * 4096 + 512);
}

static void test_bad_blocks(void) {
    uint8_t b[OTA_UF2_BLOCK_SIZE];
    ota_write_t w;

    ota_rx_t rx = start(4);
    make_block(b, 1, 4, OTA_XIP_BASE); // skipped block 0
    CHECK_EQ(ota_rx_block(&rx, b, &w), OTA_ERR_ORDER);

    rx = start(4);
    make_block(b, 0, 5, OTA_XIP_BASE); // count does not match header
    CHECK_EQ(ota_rx_block(&rx, b, &w), OTA_ERR_ORDER);

    rx = start(4);
    make_block(b, 0, 4, OTA_XIP_BASE);
    b[0] ^= 1; // bad magic
    CHECK_EQ(ota_rx_block(&rx, b, &w), OTA_ERR_UF2);

    rx = start(4);
    make_block(b, 0, 4, OTA_XIP_BASE);
    put32(b + 16, 128); // payload not 256
    CHECK_EQ(ota_rx_block(&rx, b, &w), OTA_ERR_UF2);

    rx = start(4);
    make_block(b, 0, 4, OTA_XIP_BASE);
    put32(b + 28, 0xe48bff57u); // other family
    CHECK_EQ(ota_rx_block(&rx, b, &w), OTA_ERR_FAMILY);

    rx = start(4);
    make_block(b, 0, 4, 0x20000000u); // RAM address
    CHECK_EQ(ota_rx_block(&rx, b, &w), OTA_ERR_ADDR);

    rx = start(4);
    make_block(b, 0, 4, OTA_XIP_BASE + SLOT_SIZE); // just past the slot
    CHECK_EQ(ota_rx_block(&rx, b, &w), OTA_ERR_ADDR);

    rx = start(4);
    make_block(b, 0, 4, OTA_XIP_BASE + SLOT_SIZE - 256); // last page: ok
    CHECK_EQ(ota_rx_block(&rx, b, &w), OTA_OK);

    rx = start(4);
    make_block(b, 0, 4, OTA_XIP_BASE - 256); // below XIP_BASE (wraps around)
    CHECK_EQ(ota_rx_block(&rx, b, &w), OTA_ERR_ADDR);

    rx = start(4);
    make_block(b, 0, 4, OTA_XIP_BASE + 512);
    CHECK_EQ(ota_rx_block(&rx, b, &w), OTA_OK);
    make_block(b, 1, 4, OTA_XIP_BASE + 256); // goes backwards: would rewrite flash
    CHECK_EQ(ota_rx_block(&rx, b, &w), OTA_ERR_ADDR);

    rx = start(4);
    make_block(b, 0, 4, OTA_XIP_BASE + 100); // not page aligned
    CHECK_EQ(ota_rx_block(&rx, b, &w), OTA_ERR_ADDR);
}

static void test_buy(void) {
    // Nothing pending: never act.
    CHECK_EQ(ota_buy_decide(false, true, false, false, 60000), OTA_BUY_NOTHING);

    // After USB flash: buy as soon as Bluetooth works, never revert.
    CHECK_EQ(ota_buy_decide(true, false, false, false, 1000), OTA_BUY_WAIT);
    CHECK_EQ(ota_buy_decide(true, false, false, false, 60000), OTA_BUY_WAIT);
    CHECK_EQ(ota_buy_decide(true, false, true, false, 1000), OTA_BUY_NOW);

    // After OTA: need Bluetooth and Wi-Fi, else go back to the old slot.
    CHECK_EQ(ota_buy_decide(true, true, true, false, 1000), OTA_BUY_WAIT);
    CHECK_EQ(ota_buy_decide(true, true, false, true, 1000), OTA_BUY_WAIT);
    CHECK_EQ(ota_buy_decide(true, true, true, true, 1000), OTA_BUY_NOW);
    CHECK_EQ(ota_buy_decide(true, true, true, false, OTA_BUY_DEADLINE_MS - 1), OTA_BUY_WAIT);
    CHECK_EQ(ota_buy_decide(true, true, true, false, OTA_BUY_DEADLINE_MS), OTA_BUY_REVERT);
}

static void test_replies(void) {
    CHECK(memcmp(ota_status_reply(OTA_ERR_TOKEN), "ETOK", 4) == 0);
    CHECK(memcmp(ota_status_reply((ota_status_t)999), "E???", 4) == 0);
    for (int s = OTA_OK; s <= OTA_ERR_PROTOCOL; s++) {
        CHECK_EQ(strlen(ota_status_reply((ota_status_t)s)), OTA_REPLY_SIZE);
    }
}

int main(void) {
    test_header();
    test_too_big();
    test_erase_and_offsets();
    test_gap();
    test_bad_blocks();
    test_buy();
    test_replies();
    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("ota_logic: all passed\n");
    return 0;
}
