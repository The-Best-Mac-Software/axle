#include "paging.h"
#include <std/kheap.h>
#include <std/std.h>
#include <kernel/kernel.h>
#include <std/printf.h>
#include <gfx/lib/gfx.h>

//bitset of frames - used or free
uint32_t* frames;
uint32_t nframes;

page_directory_t* kernel_directory = 0;
page_directory_t* current_directory = 0;

volatile uint32_t memsize = 0; //size of paging memory

//defined in kheap
extern uint32_t placement_address;
extern heap_t* kheap;

extern uint32_t end;

//macros used in bitset algorithms
#define BITS 8
#define INDEX_FROM_BIT(a) (a / (BITS * sizeof(*frames)))
#define OFFSET_FROM_BIT(a) (a % (BITS * sizeof(*frames)))

uint32_t get_cr0() {
	uint32_t cr0;
	asm volatile("mov %%cr0, %0" : "=r"(cr0));
	return cr0;
}

void set_cr0(uint32_t cr0) {
	asm volatile("mov %0, %%cr0" : : "r"(cr0));
}

page_directory_t* get_cr3() {
	uint32_t cr3;
	asm volatile("mov %%cr3, %0" : "=r"(cr3));
	return (page_directory_t*)cr3;
}

void set_cr3(page_directory_t* dir) {
	printf("cr3 is %x\n", &dir->tablesPhysical);
	asm volatile("mov %0, %%cr3":: "r"(&dir->tablesPhysical));
}

//static function to set a bit in frames bitset
static void set_bit_frame(uint32_t frame_addr) {
	uint32_t frame = frame_addr/0x1000;
	uint32_t idx = INDEX_FROM_BIT(frame);
	uint32_t off = OFFSET_FROM_BIT(frame);
	frames[idx] |= (0x1 << off);
}

//static function to clear a bit in the frames bitset
static void clear_frame(uint32_t frame_addr) {
	uint32_t frame = frame_addr/0x1000;
	uint32_t idx = INDEX_FROM_BIT(frame);
	uint32_t off = OFFSET_FROM_BIT(frame);
	frames[idx] &= ~(0x1 << off);
}

//static function to test if a bit is sset
static uint32_t test_frame(uint32_t frame_addr) {
	uint32_t frame = frame_addr/0x1000;
	uint32_t idx = INDEX_FROM_BIT(frame);
	uint32_t off = OFFSET_FROM_BIT(frame);
	return (frames[idx] & (0x1 << off));
}

//static function to find the first free frame
static int32_t first_frame() {
	for (uint32_t i = 0; i < INDEX_FROM_BIT(nframes); i++) {
		if (frames[i] != 0xFFFFFFFF) {
			//at least one free bit
			for (uint32_t j = 0; j < 32; j++) {
				uint32_t bit = 0x1 << j;
				if (!(frames[i] & bit)) {
					//found unused bit i in addr
					return i*4*8+j;
				}
			}
		}
	}
	printf_info("first_frame(): no free frames!");
	return -1;
}

void virtual_map_pages(long addr, unsigned long size, uint32_t rw, uint32_t user) {
	unsigned long i = addr;
	while (i < (addr + size + 0x1000)) {
		if (i + size < memsize) {
			//find first free frame
			set_bit_frame(first_frame());

			//set space to taken anyway
			kmalloc(0x1000);
		}

		page_t* page = get_page(i, 1, current_directory);
		page->present = 1;
		page->rw = rw;
		page->user = user;
		page->frame = i / 0x1000;
		i += 0x1000;
	}
	return;
}

void vmem_map(uint32_t virt, uint32_t physical, page_directory_t* dir) {
	printf_info("Mapping %x -> %x", virt, physical);
	//turn address into an index
	virt /= 0x1000;
	//find page table containing this address
	uint32_t table_idx = virt / 1024;

	//if this page is already assigned
	if (dir->tables[table_idx]) {
		//we need to take this frame
		//copy the current frame data into a free page
		page_t* new = get_page(first_frame(), 1, dir);
		memcpy(new, dir->tables[table_idx], 0x1000);
		//dir->tables[table_idx]->frame = NULL;
	}

	page_t* mapped = alloc_frame(get_page(physical, 1, dir), 0, 0);
}

//function to allocate a frame
int alloc_frame(page_t* page, int is_kernel, int is_writeable) {
	if (page->frame) {
		//frame was already allocated!
		printf_dbg("alloc_page(): %x already mapped to %x", page, page->frame);
		return -1;
	}

	int32_t idx = first_frame(); //index of first free frame
	if (idx == -1) {
		PANIC("No free frames!");
	}
	//printf_info("alloc_frame(): setting bit frame %x", idx);
	set_bit_frame(idx*0x1000); //frame is now ours
	page->present = 1; //mark as present
	page->rw = is_writeable; //should page be writable?
	page->user = !is_kernel; //should page be user mode?
	page->frame = idx;

	return 0;
}

//function to dealloc a frame
void free_frame(page_t* page) {
	uint32_t frame;
	if (!(frame = page->frame)) {
		//page didn't actually have an allocated frame!
		return;
	}
	clear_frame(frame); //frame is now free again
	page->frame = 0x0; //page now doesn't have a frame
}

#define VESA_WIDTH 1024
#define VESA_HEIGHT 768
void identity_map_lfb(uint32_t location) { 

	//VESA has 3 bytes/pixel
	int bpp = 3;
	int required_bytes = VESA_WIDTH * VESA_HEIGHT * bpp;
	uint32_t j = location;
	//TODO use screen object instead of these vals
	while (j < location + required_bytes) {
		//if frame is valid
		if (j + location + required_bytes < memsize) {
			set_bit_frame(j); //tell frame bitset this frame is in use
		}
		//get page
		page_t* page = get_page(j, 1, kernel_directory);
		//fill it
		page->present = 1;
		page->rw = 1;
		page->user = 1;
		page->frame = j / 0x1000;
		j += 0x1000;
	}
	//virtual_map_pages(location, required_bytes, 1, 1);
}

static void page_fault(registers_t regs);

void set_paging_bit(bool enabled) {
	if (enabled) {
		set_cr0(get_cr0() | 0x80000000);
	}
	else {
		set_cr0(get_cr0() & 0x7FFFFFFF);
	}
}

static uint32_t* page_directory = 0;
static uint32_t page_dir_loc = 0;
static uint32_t* last_page = 0;

void paging_map_virt_to_phys(uint32_t virt, uint32_t phys) {
	uint16_t id = virt >> 22;
	for (int i = 0; i < 1024; i++) {
		last_page[i] = phys | 3;
		phys += 0x1000;
	}
	page_directory[id] = ((uint32_t)last_page) | 3;
	last_page = (uint32_t*)(((uint32_t)last_page) + 0x1000);
	printf("mapping %x (%d) to %x\n", virt, id, phys);
}

void paging_install() {
	printf_info("Initializing paging...");

	//size of physical memory
	//assume 32MB
	uint32_t mem_end_page = 0x1000000;
	//uint32_t mem_end_page = memory_size;
	memsize = mem_end_page;

	printf("1");

	nframes = mem_end_page / 0x1000;
	frames = (uint32_t*)kmalloc(INDEX_FROM_BIT(nframes));
	memset(frames, 0, INDEX_FROM_BIT(nframes));

	printf("2");

	//make page directory
	kernel_directory = (page_directory_t*)kmalloc_a(sizeof(page_directory_t));
	memset(kernel_directory, 0, sizeof(page_directory_t));
	kernel_directory->physicalAddr = (uint32_t)kernel_directory->tablesPhysical;

	printf("3 dir %x\n", kernel_directory);

	while (1) {}

	//identity map VESA LFB
	//uint32_t vesa_mem_addr = 0xFD000000; //TODO replace with function
	//identity_map_lfb(vesa_mem_addr);

	//map pages in kernel heap area
	//we call get_page but not alloc_frame
	//this causes page_table_t's to be created where necessary
	//don't alloc the frames yet, they need to be identity
	//mapped below first.
	/*
    unsigned int i = 0;
	for (i = KHEAP_START; i < KHEAP_START + KHEAP_INITIAL_SIZE; i += 0x1000) {
		get_page(i, 1, kernel_directory);
	}
	printf("4");
	*/

	//we need to identity map (phys addr = virtual addr) from
	//0x0 to end of used memory, so we can access this
	//transparently, as if paging wasn't enabled
	//note, inside this loop body we actually change placement_address
	//by calling kmalloc(). A while loop causes this to be computed
	//on-the-fly instead of once at the start
	uint32_t* kernel_base = 0xC0000000;
	unsigned idx = 0;
	while (idx < 0x100000) {
		vmem_map(kernel_base + idx, idx, kernel_directory);
		//kernel code is readable but not writeable from userspace
		//int get = alloc_frame(get_page(idx, 1, kernel_directory), 0, 0);
		/*
		if (get < 0) {
			PANIC("Getting page at %x failed", idx);
		}
		*/
		idx += 0x1000;
	}

	/*
	//allocate pages we mapped earlier
	for (i = KHEAP_START; i < KHEAP_START + KHEAP_INITIAL_SIZE; i += 0x1000) {
		alloc_frame(get_page(i, 1, kernel_directory), 0, 0);
	}
	*/
	
	printf_info("finished identity mapping kernel pages");

	//before we enable paging, register page fault handler
	register_interrupt_handler(14, page_fault);

	//enable paging
	printf("loading %x into cr3", kernel_directory);

	enter_higher_half();

}

void paging_enable() {
	switch_page_directory(kernel_directory);
	//turn on paging
	set_paging_bit(true);
	while (1) {}

	printf("attempting heap..\n");
	//initialize kernel heap
	//kheap = create_heap(KHEAP_START, KHEAP_START + KHEAP_INITIAL_SIZE, KHEAP_MAX_ADDRESS, 0, 0); 
	//kheap = create_heap(KHEAP_START, KHEAP_START + KHEAP_INITIAL_SIZE, 0xCFFFF000, 0, 0); 
	printf("made heap\n");
	//expand(0x1000000, kheap);

	//current_directory = clone_directory(kernel_directory);
	//current_directory = kernel_directory;
	//switch_page_directory(current_directory);
}

void switch_page_directory(page_directory_t* dir) {
	current_directory = dir;
	set_cr3(dir);
}

page_t* get_page(uint32_t address, int make, page_directory_t* dir) {
	//turn address into an index
	address /= 0x1000;
	//find page table containing this address
	uint32_t table_idx = address / 1024;

	//if this page is already assigned
	if (dir->tables[table_idx]) {
		return &dir->tables[table_idx]->pages[address%1024];
	}
	else if (make) {
		uint32_t tmp;
		dir->tables[table_idx] = (page_table_t*)kmalloc_ap(sizeof(page_table_t), &tmp);
		memset(dir->tables[table_idx], 0, 0x1000);
		//PRESENT, RW, US
		dir->tablesPhysical[table_idx] = tmp | 0x7;
		return &dir->tables[table_idx]->pages[address%1024];
	}
	return 0;
}

static void page_fault(registers_t regs) {
	switch_to_text();

	//page fault has occured
	//faulting address is stored in CR2 register
	uint32_t faulting_address;
	asm volatile("mov %%cr2, %0" : "=r" (faulting_address));

	//error code tells us what happened
	int present = !(regs.err_code & 0x1); //page not present
	int rw = regs.err_code & 0x2; //write operation?
	int us = regs.err_code & 0x4; //were we in user mode?
	int reserved = regs.err_code & 0x8; //overwritten CPU-reserved bits of page entry?
	int id = regs.err_code & 0x10; //caused by instruction fetch?

	printf_err("Encountered page fault at %x", faulting_address);

	if (present) printf_err("Page present");
	else printf_err("Page not present");

	if (rw) printf_err("Write operation");
	else printf_err("Read operation");

	if (us) printf_err("User mode");
	else printf_err("Supervisor mode");

	if (reserved) printf_err("Overwrote CPU-resereved bits of page entry");

	if (id) printf_err("Faulted during instruction fetch");

	if (regs.eip != faulting_address) {
		printf_err("Executed unpaged memory");
	}
	else {
		printf_err("Read unpaged memory");
	}
	
	//if accessing a non-mapped page, map it
	if (!present) {
		printf_info("Attempting page fault recovery...");
		alloc_frame(get_page(faulting_address, true, kernel_directory), us, rw);

		return;
	}

	extern void common_halt(registers_t regs, bool recoverable);
	common_halt(regs, false);
}

static page_table_t* clone_table(page_table_t* src, uint32_t* physAddr) {
	//make new page aligned table
	page_table_t* table = (page_table_t*)kmalloc_ap(sizeof(page_table_t), physAddr);
	//ensure new table is blank
	memset((uint8_t*)table, 0, sizeof(page_table_t));

	//for each entry in table
	for (int i = 0; i < 1024; i++) {
		//if source entry has a frame associated with it
		if (!src->pages[i].frame) continue;

		//get new frame
		alloc_frame(&table->pages[i], 0, 0);
		//clone flags from source to destination
		table->pages[i].present = src->pages[i].present;
		table->pages[i].rw = src->pages[i].rw;
		table->pages[i].user = src->pages[i].user;
		table->pages[i].accessed = src->pages[i].accessed;
		table->pages[i].dirty = src->pages[i].dirty;

		//physically copy data across
		extern void copy_page_physical(uint32_t page, uint32_t dest);
		copy_page_physical(src->pages[i].frame * 0x1000, table->pages[i].frame * 0x1000);
	}
	return table;
}

page_directory_t* clone_directory(page_directory_t* src) {
	uint32_t phys;

	//make new page directory and get physaddr
	page_directory_t* dir = (page_directory_t*)kmalloc_ap(sizeof(page_directory_t), &phys);
	//blank it
	memset((uint8_t*)dir, 0, sizeof(page_directory_t));

	//get offset of tablesPhysical from start of page_directory_t
	uint32_t offset = (uint32_t)dir->tablesPhysical - (uint32_t)dir;
	dir->physicalAddr = phys + offset;

	//for each page table
	//if in kernel directory, don't make copy
	for (int i = 0; i < 1024; i++) {
		if (!src->tables[i]) continue;

		if (kernel_directory->tables[i] == src->tables[i]) {
			//in kernel, so just reuse same pointer (link)
			dir->tables[i] = src->tables[i];
			dir->tablesPhysical[i] = src->tablesPhysical[i];
		}
		else {
			//copy table
			uint32_t phys;
			dir->tables[i] = clone_table(src->tables[i], &phys);
			dir->tablesPhysical[i] = phys | 0x07;
		}
	}
	return dir;
}

void free_directory(page_directory_t* dir) {
	//first free all tables
	for (int i = 0; i < 1024; i++) {
		page_table_t* table = dir->tables[i];
		//free all pages in table
		for (int j = 0; j < 1024; j++) {
			page_t page = table->pages[j];
			free_frame(&page);
		}

		//free table itself
		kfree(table);
	}
	//finally, free directory
	kfree(dir);
}

uint32_t vmm_get_mapping(uint32_t addr) {
	page_t* page = get_page((uint32_t)addr, 0, current_directory);
	if (!page || !page->frame) {
		return NULL;
	}
	uint32_t phys = page->frame * 0x1000 + ((uint32_t)addr & 0xFFF);
	return phys;
}

