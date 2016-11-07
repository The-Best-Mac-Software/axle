#include "vmm.h"
#include <kernel/util/interrupts/isr.h>
#include <kernel/util/pmm/pmm.h>
#include <std/panic.h>
#include <std/printf.h>
#include <std/memory.h>

uint32_t* page_directory = (uint32_t*)PAGE_DIR_VIRT_ADDR;
uint32_t* page_tables = (uint32_t*)PAGE_TABLE_VIRT_ADDR;

page_directory_t* current_directory;
extern bool pmm_paging_active;

void page_fault(registers_t regs);

void paging_install() {
	kernel_begin_critical();
	printf_info("Initializing paging...");

	uint32_t cr0;

	//page fault handler
	register_interrupt_handler(14, &page_fault);

	//create initial page directory
	page_directory_t* p_dir = (page_directory_t*)pmm_alloc_page();
	memset(p_dir, 0, PAGE_SIZE);

	//identity map first 4MB
	p_dir[0] = pmm_alloc_page() | PAGE_PRESENT | PAGE_WRITE;
	uint32_t* p_table = (uint32_t*)(p_dir[0] & PAGE_MASK);
	//initialize all pages in table
	for (int i = 0; i < 1024; i++) {
		p_table[i] = i * PAGE_SIZE | PAGE_PRESENT | PAGE_WRITE;
	}

	//assign second-to-last table
	p_dir[1022] = pmm_alloc_page() | PAGE_PRESENT | PAGE_WRITE;
	p_table = (uint32_t*)(p_dir[1022] & PAGE_MASK);
	memset(p_table, 0, PAGE_SIZE);

	//last entry of second-to-last table is page directory itself
	p_table[1023] = (uint32_t)p_dir | PAGE_PRESENT | PAGE_WRITE;

	//very last page table loops back directory on itself
	p_dir[1023] = (uint32_t)p_dir | PAGE_PRESENT | PAGE_WRITE;

	//paging dir initialized, start using it
	switch_page_directory(p_dir);

	//enable paging
	asm volatile("mov %%cr0, %0" : "=r"(cr0));
	cr0 |= 0x80000000;
	asm volatile("mov %0, %%cr0" : : "r"(cr0));

	//we must map page table where physical mem manager keeps its page stack,
	//or else it'll just panic on the first pmm_free_page() 
	uint32_t pt_idx = PAGE_DIR_IDX((PMM_STACK_ADDR >> 12));
	page_directory[pt_idx] = pmm_alloc_page() | PAGE_PRESENT | PAGE_WRITE;
	memset(page_tables[pt_idx * 1024], 0, PAGE_SIZE);

	//paging is active!
	//let physical mem manager know
	pmm_paging_active = true;

	printf_info("paging successfully initialized!");
	kernel_end_critical();
}

void switch_page_directory(page_directory_t* pd) {
	current_directory = pd;
	//load addr of new paging dir into cr3
	asm volatile("mov %0, %%cr3" : : "r"(pd));
}

void vmm_map(uint32_t virt, uint32_t page, uint32_t flags) {
	uint32_t virt_page = virt / PAGE_SIZE;
	uint32_t pt_idx = PAGE_DIR_IDX(virt_page);

	//find appropriate page table
	if (!page_directory[pt_idx]) {
		//page table holding this page hasn't been created yet!
		page_directory[pt_idx] = pmm_alloc_page() | PAGE_PRESENT | PAGE_WRITE;
		memset(page_tables[pt_idx * 1024], 0, PAGE_SIZE);
	}

	//page table exists, update PTE
	page_tables[virt_page] = (page & PAGE_MASK) | flags;
}

void vmm_unmap(uint32_t virt) {
	uint32_t virt_page = virt / PAGE_SIZE;

	page_tables[virt_page] = 0;

	//invalidated a page mapping, inform CPU
	asm volatile("invlpg (%0)" : : "a"(virt));
}

bool vmm_get_mapping(uint32_t virt, uint32_t* phys) {
	uint32_t virt_page = virt / PAGE_SIZE;
	uint32_t pt_idx = PAGE_DIR_IDX(virt_page);

	//find page table
	if (!page_directory[pt_idx]) {
		return false;
	}

	if (page_tables[virt_page]) {
		if (phys) {
			*phys = page_tables[virt_page] & PAGE_MASK;
		}
		return true;
	}
	return false;
}

void page_fault(registers_t regs) {
	uint32_t cr2;
	asm volatile("mov %%cr2, %0" : "=r"(cr2));

	printf("page fault at %x, faulting addr %x\n", regs.eip, cr2);
	printf("error code: %x\n", regs.err_code);
	printf("halting execution...");
	ASSERT(0, "page fault");
}

