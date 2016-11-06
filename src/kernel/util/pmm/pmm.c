#include "pmm.h"
#include <kernel/util/vmm/vmm.h>
#include <std/std.h>

uint32_t pmm_stack_loc = PMM_STACK_ADDR;
uint32_t pmm_stack_max = PMM_STACK_ADDR;
uint32_t pmm_loc;
bool pmm_paging_active = false;

void pmm_install(uint32_t start_loc) {
	//ensure start addr is page-aligned
	pmm_loc = (start_loc + PAGE_SIZE) & PAGE_MASK;
}

uint32_t pmm_alloc_page() {
	if (pmm_paging_active) {
		if (pmm_stack_loc == PMM_STACK_ADDR) {
			ASSERT(0, "pmm out of memory!");
		}

		//pop off stack
		pmm_stack_loc -= sizeof(uint32_t);
		uint32_t* stack = (uint32_t*)pmm_stack_loc;

		return *stack;
	}
	else {
		return pmm_loc += PAGE_SIZE;
	}
}
void pmm_free_page(uint32_t page) {
	//if addr is under loc, do nothing!
	//this region might have important boot data
	//(such as, for example, paging structs :p)
	if (page < pmm_loc) return;

	//no stack space
	if (pmm_stack_max <= pmm_stack_loc) {
		//map page we're freeing at top of free page stack
		vmm_map(pmm_stack_max, page, PAGE_PRESENT | PAGE_WRITE);
		//increase free page stack by a page
		pmm_stack_max += PAGE_SIZE;
	}
	else {
		//stack has free space
		//so, push
		uint32_t* stack = (uint32_t*)pmm_stack_loc;
		*stack = page;
		pmm_stack_loc += sizeof(uint32_t);
	}
}
