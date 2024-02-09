#ifndef __LINUX_LL_H
#define __LINUX_LL_H

#include <stdint.h>

void init_linux_mmap(void);
int linux_flash_read(void *data, uint32_t addr, uint32_t size);

#endif
