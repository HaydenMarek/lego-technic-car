// Fix: the BTstack flash store (pairing keys, our settings) must work when we
// run from a partition.
//
// BTstack keeps pairing keys in the last 8 KB of flash (pico_btstack_flash_bank).
// The SDK erases/writes them by PHYSICAL flash offset, but reads them through
// XIP_BASE + offset. When the boot ROM starts us from partition A or B, it
// turns on address translation (ATRANS): XIP_BASE then means "start of our
// partition", so the read hits the wrong place.
//
// The SDK's write has the same bug: to change a few bytes it re-reads the
// rest of the 256-byte page through XIP_BASE. The bank is at the end of the
// 4 MB flash, outside our 2000 KB partition window, so that read faults: the
// CPU stops and the watchdog resets the Pico. Seen as "car restarts after a
// D-pad gain change" (first TLV write since the partitions, 2026-11-11).
//
// The linker option -Wl,--wrap=pico_flash_bank_instance (CMakeLists.txt)
// sends the SDK's call to us. We reuse the SDK's erase (physical offsets)
// and replace read and write with ones through the untranslated XIP window.

#include <string.h>

#include <hardware/flash.h>
#include <hardware/regs/addressmap.h>
#include <pico/btstack_flash_bank.h>
#include <pico/flash.h>

const hal_flash_bank_t *__real_pico_flash_bank_instance(void);

#define BANK_SIZE (PICO_FLASH_BANK_TOTAL_SIZE / 2)

// Physical flash, whatever partition we run from. NOCACHE: never see stale
// cache lines after an erase/write.
static const uint8_t *untranslated(uint32_t flash_offset) {
    return (const uint8_t *)(XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE + flash_offset);
}

static void read_untranslated(void *context, int bank, uint32_t offset, uint8_t *buffer,
                              uint32_t size) {
    (void)context;
    // Same checks as the SDK's pico_flash_bank_read().
    if (bank < 0 || bank > 1 || offset >= BANK_SIZE || offset + size > BANK_SIZE) {
        return;
    }
    memcpy(buffer, untranslated(PICO_FLASH_BANK_STORAGE_OFFSET + BANK_SIZE * bank + offset), size);
}

typedef struct {
    uint32_t flash_offset; // page start, physical
    const uint8_t *data;   // FLASH_PAGE_SIZE bytes
} page_program_t;

static void program_page(void *param) {
    const page_program_t *p = param;
    flash_range_program(p->flash_offset, p->data, FLASH_PAGE_SIZE);
}

// Like the SDK's pico_flash_bank_write(), page by page: old page contents
// (read untranslated) + the new bytes, then program the whole page.
// Re-programming unchanged bytes is harmless on NOR flash.
static void write_untranslated(void *context, int bank, uint32_t offset, const uint8_t *data,
                               uint32_t size) {
    (void)context;
    if (bank < 0 || bank > 1 || offset >= BANK_SIZE || offset + size > BANK_SIZE || size == 0) {
        return;
    }
    uint32_t bank_start = PICO_FLASH_BANK_STORAGE_OFFSET + BANK_SIZE * bank;
    while (size > 0) {
        uint32_t page_start = offset - offset % FLASH_PAGE_SIZE;
        uint32_t in_page = offset - page_start;
        uint32_t n = FLASH_PAGE_SIZE - in_page;
        if (n > size) {
            n = size;
        }

        uint8_t page[FLASH_PAGE_SIZE];
        memcpy(page, untranslated(bank_start + page_start), FLASH_PAGE_SIZE);
        memcpy(page + in_page, data, n);

        page_program_t p = {.flash_offset = bank_start + page_start, .data = page};
        // Not nested: we are called from the BTstack loop, never from inside
        // another flash_safe_execute() (see KNOWLEDGE.md, rom_explicit_buy).
        flash_safe_execute(program_page, &p, UINT32_MAX);

        offset += n;
        data += n;
        size -= n;
    }
}

const hal_flash_bank_t *__wrap_pico_flash_bank_instance(void) {
    static hal_flash_bank_t fixed;
    if (fixed.read == NULL) {
        fixed = *__real_pico_flash_bank_instance();
        fixed.read = read_untranslated;
        fixed.write = write_untranslated;
    }
    return &fixed;
}
