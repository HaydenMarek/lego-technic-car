// Fix: Bluetooth pairing keys must be readable when we run from a partition.
//
// BTstack keeps pairing keys in the last 8 KB of flash (pico_btstack_flash_bank).
// The SDK erases/writes them by PHYSICAL flash offset, but reads them through
// XIP_BASE + offset. When the boot ROM starts us from partition A or B, it
// turns on address translation (ATRANS): XIP_BASE then means "start of our
// partition", so the read hits the wrong place.
//
// The linker option -Wl,--wrap=pico_flash_bank_instance (CMakeLists.txt)
// sends the SDK's call to us. We reuse the SDK's erase/write and only
// replace read with one through the untranslated XIP window.

#include <string.h>

#include <hardware/regs/addressmap.h>
#include <pico/btstack_flash_bank.h>

const hal_flash_bank_t *__real_pico_flash_bank_instance(void);

#define BANK_SIZE (PICO_FLASH_BANK_TOTAL_SIZE / 2)

static void read_untranslated(void *context, int bank, uint32_t offset, uint8_t *buffer,
                              uint32_t size) {
    (void)context;
    // Same checks as the SDK's pico_flash_bank_read().
    if (bank < 0 || bank > 1 || offset >= BANK_SIZE || offset + size > BANK_SIZE) {
        return;
    }
    // NOCACHE: never see stale cache lines after an erase/write.
    // NOTRANSLATE: physical flash address, whatever partition we run from.
    const uint8_t *src = (const uint8_t *)(XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE +
                                           PICO_FLASH_BANK_STORAGE_OFFSET + BANK_SIZE * bank +
                                           offset);
    memcpy(buffer, src, size);
}

const hal_flash_bank_t *__wrap_pico_flash_bank_instance(void) {
    static hal_flash_bank_t fixed;
    if (fixed.read == NULL) {
        fixed = *__real_pico_flash_bank_instance();
        fixed.read = read_untranslated;
    }
    return &fixed;
}
