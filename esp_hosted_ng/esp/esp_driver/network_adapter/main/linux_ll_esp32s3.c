#include "sdkconfig.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "linux_ll.h"
#include "hal/cache_hal.h"
#include "hal/mmu_ll.h"

static uint32_t first_free_mmu = ~0;

void init_linux_mmap(void)
{
	volatile uint32_t *dst = (uint32_t *)DR_REG_MMU_TABLE;
	bool first = true;
	int i;

	for (i = 0; i < 0x100; ++i) {
		if (dst[i] & 0xc000) {
			if (first) {
				first = false;
				first_free_mmu = i;
			}
			dst[i] = i;
		} else if (!first) {
			abort();
		}
	}
#ifdef CONFIG_LINUX_PSRAM_8M
	for (i = 0x100; i < 0x180; ++i) {
		dst[i] = 0x4000;
	}
	for (i = 0x180; i < 0x200; ++i) {
		dst[i] = 0x8000 + (i & 0x7f);
	}
#endif
#ifdef CONFIG_LINUX_PSRAM_16M
	/* linearly map PSRAM to the range +16M..+32M */
	for (i = 0x100; i < 0x200; ++i) {
		dst[i] = 0x8000 + (i & 0xff);
	}
#endif
}

int linux_flash_read(void *data, uint32_t addr, uint32_t size)
{
	uint32_t vaddr = 0x3c000000 + first_free_mmu * 0x10000 + addr % 0x10000;

	((volatile uint32_t *)DR_REG_MMU_TABLE)[first_free_mmu] = addr / 0x10000;
	cache_hal_invalidate_addr(vaddr, size);
	memcpy(data, (void *)vaddr, size);

	return ESP_OK;
}
