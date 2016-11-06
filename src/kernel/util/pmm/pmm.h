#ifndef PMM_H
#define PMM_H

#include <stdint.h>

#define PMM_STACK_ADDR 0xFF000000

void pmm_install(uint32_t start_loc);
uint32_t pmm_alloc_page();
void pmm_free_page(uint32_t page);

#endif
