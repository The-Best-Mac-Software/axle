#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stdbool.h>

#define PAGE_DIR_VIRT_ADDR		0xFFBFF000
#define PAGE_TABLE_VIRT_ADDR	0xFFC00000
#define PAGE_DIR_IDX(x)			((uint32_t)(x) / 1024)
#define PAGE_TABLE_IDX(x)		((uint32_t)(x) % 1024)

#define PAGE_PRESENT	0x1
#define PAGE_WRITE		0x2
#define PAGE_USER		0x4
#define PAGE_MASK		0xFFFFF000

#define PAGE_SIZE 0x1000

typedef uint32_t page_directory_t;

//sets up paging structures and enables paging
void paging_install();

//switch address space
void switch_page_directory(page_directory_t* pd);

//maps physical page 'page' into virtual address space at address 'virt'
//using page flags 'flags'
void vmm_map(uint32_t virt, uint32_t page, uint32_t flags);

//removes single page of virtual->physical mapping at virtual addr 'virt'
void vmm_unmap(uint32_t virt);

//true if virtual addr 'virt' is mapped in current address space
//physical address of mapping is placed in 'phys'
bool vmm_get_mapping(uint32_t virt, uint32_t* phys);

#endif

