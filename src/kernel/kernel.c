#include <kernel/kernel.h>
#include "multiboot.h"
#include <stdarg.h>
#include <std/common.h>
#include <std/kheap.h>
#include <std/printf.h>
#include <user/shell/shell.h>
#include <user/xserv/xserv.h>
#include <gfx/lib/gfx.h>
#include <gfx/lib/view.h>
#include <gfx/font/font.h>
#include <kernel/util/syscall/syscall.h>
#include <kernel/util/syscall/sysfuncs.h>
#include <kernel/util/paging/descriptor_tables.h>
#include <kernel/util/paging/paging.h>
#include <kernel/util/multitasking/tasks/task.h>
#include <kernel/util/mutex/mutex.h>
#include <kernel/util/vfs/initrd.h>
#include <kernel/drivers/rtc/clock.h>
#include <kernel/drivers/pit/pit.h>
#include <kernel/drivers/mouse/mouse.h>
#include <kernel/drivers/vesa/vesa.h>
#include <kernel/drivers/pci/pci_detect.h>
#include <tests/test.h>

void print_os_name(void) {
	printf("\e[10;[\e[11;AXLE OS v\e[12;0.6.0\e[10;]\n");
}

/*
void shell_loop(void) {
	int exit_status = 0;
	while (!exit_status) {
		exit_status = shell();
	}

	//give them a chance to recover
	for (int i = 5; i > 0; i--) {
		terminal_clear();
		printf_info("Shutting down in %d. Press any key to cancel", i);
		sleep(1000);
		if (haskey()) {
			//clear buffer
			while (haskey()) {
				getchar();
			}
			//restart shell loop
			terminal_clear();
			shell_loop();
			break;
		}
	}

	//we're dead
	terminal_clear();
}

extern uint32_t placement_address;

uint32_t module_detect(multiboot* mboot_ptr) {
	printf_info("Detected %d GRUB modules", mboot_ptr->mods_count);
	ASSERT(mboot_ptr->mods_count > 0, "no GRUB modules detected");
	uint32_t initrd_loc = *((uint32_t*)mboot_ptr->mods_addr);
	uint32_t initrd_end = *(uint32_t*)(mboot_ptr->mods_addr+4);
	//don't trample modules
	placement_address = initrd_end;
	return initrd_loc;
}

*/

static void read_boot_info(uint32_t magic, uint32_t initial_stack) {
	printf("initial stack @ %x magic %x ", initial_stack, magic);
	enum {
		GRUB_MAGIC = 0x2BADB002,
	};
	switch (magic) {
		case GRUB_MAGIC:
			printf("(grub) ");
			break;
		default:
			printf("(unkwn) ");
			break;
	}
	printf("\n");
}

struct multiboot_mmap_entry {
       uint32_t size;
	   uint32_t addr_low;
	   uint32_t addr_high;
	   uint32_t len_low;
	   uint32_t len_high;
       uint32_t type;
} __attribute__((packed));
typedef struct multiboot_mmap_entry multiboot_memory_map_t;

static void read_mmap(multiboot* mbi) {
	printf("---memory map---\n");
	for (multiboot_memory_map_t* mmap = (multiboot_memory_map_t *)(mbi->mmap_addr + KERN_VMA); 
		 (unsigned long)mmap < (mbi->mmap_addr + mbi->mmap_length + KERN_VMA); 
		 mmap = (multiboot_memory_map_t *) ((unsigned long) mmap + mmap->size + sizeof (mmap->size))) {
		printf("mem region @ %x, %x bytes, ", mmap->addr_low, mmap->len_low);
		switch (mmap->type) {
			case 1:
				printf("(free %d)\n", mmap->type);
				break;
			case 2:
				printf("(reserved %d)\n", mmap->type);
				break;
		}
	}
}

uint32_t initial_esp;
void kernel_main(multiboot* mboot_ptr, uint32_t magic, uint32_t initial_stack) {
	initial_esp = initial_stack;

	//initialize terminal interface
	terminal_initialize();

	//introductory message
	print_os_name();
	//test_colors();
	read_boot_info(magic, initial_stack);
	read_mmap(mboot_ptr);

	printf_info("Available memory:");
	printf("%d -> %dMB\n", mboot_ptr->mem_upper, (mboot_ptr->mem_upper/1024));

	//descriptor tables
	gdt_install();
	idt_install();

	//test_interrupts();

	//timer driver (many functions depend on timer interrupt so start early)
	kernel_end_critical();
	pit_install(1000);
	while (tick_count() < 10) {}
	printf("ticks: %d\n", tick_count());
	printf("gdt/idt ok\n");
	printf("interrupts ok\n");
	/*

	//find any loaded grub modules
	//uint32_t initrd_loc = module_detect(mboot_ptr);

	//utilities
	kernel_begin_critical();
	paging_install();
	extern heap_t* kheap;

	uint32_t* ptr = (uint32_t*)0xA0000000;
	uint32_t force_fault = *ptr;
/*
	void* a = kmalloc(64);
	heap_print(kheap);

	kfree(a);
	heap_print(kheap);
	*/
	//heap_print();

	/*
	sys_install();
	// tasking_install(PRIORITIZE_INTERACTIVE);
	tasking_install(LOW_LATENCY);

	//drivers
	*/
	kb_install();
	mouse_install();
	//pci_install();
	char ch = NULL;
	while (1) {
		asm("hlt");
		ch = kgetch();
		if (ch) {
			printf("%c", ch);
		}
		ch = NULL;
	}
	/*

	//initialize initrd, and set as fs root
	fs_root = initrd_install(initrd_loc);

	//test facilities
	//test_heap();
	test_printf();
	test_time_unique();
	test_malloc();
	test_crypto();
	//heap_int_test();

	//Bmp* test = load_jpeg(rect_make(point_zero(), size_make(200, 200)), "./jpeg_test.jpg");
	

	if (!fork("shell")) {
		//start shell
		shell_init();
		shell_loop();
	}

	if (!fork("sleepy")) {
		sleep(20000);
		printf_dbg("Sleepy thread slept!");
		_kill();
	}

	//done bootstrapping, kill process
	_kill();
	*/

	//this should never be reached as the above call is never executed
	//if for some reason it is, just spin
	while (1) {
		sys_yield(RUNNABLE);
	}

	//if by some act of god we've reached this point, just give up and assert
	ASSERT(0, "Kernel exited");
}
