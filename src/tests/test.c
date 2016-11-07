#include "test.h"
#include <std/std.h>
#include <kernel/drivers/terminal/terminal.h>
#include <kernel/drivers/vesa/vesa.h>
#include <kernel/drivers/rtc/clock.h>
#include <crypto/crypto.h>
#include <kernel/util/pmm/pmm.h>
#include <kernel/util/vmm/vmm.h>

void test_colors() {
	printf_info("Testing colors...");
	for (int i = 0; i < 16; i++) {
		printf("\e[%d;@", i);
	}
	printf("\n");
}

void force_hardware_irq() {
	printf_info("Forcing hardware IRQ...");
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiv-by-zero"
	int i = 500/0;
#pragma GCC diagnostic pop
	printf_dbg("%d", i);
}

void force_page_fault() {
	printf_info("Forcing page fault...");
	uint32_t* ptr = (uint32_t*)0xA0000000;
	uint32_t do_fault = *ptr;
	printf_err("This should never be reached: %d", do_fault);
}

void test_interrupts() {
	printf_info("Testing interrupts...");
	asm volatile("mov $0xdeadbeef, %eax");
	asm volatile("mov $0xcafebabe, %ecx");
	asm volatile("int $0x3");
	//asm volatile("int $0x4");
}

void test_heap() {
	printf_info("Testing heap...");
	void* a = kmalloc(8);
	void* b = kmalloc(8);
	void* c = kmalloc(8);
	kfree(a);
	kfree(b);
	void* d = kmalloc(24);
	kfree(c);
	kfree(d);
	printf("a: %x, b: %x, c: %x, d: %x\n", a, b, c, d);
}

void test_vmm() {
	uint32_t page = pmm_alloc_page();
	uint32_t* ptr = (uint32_t*)page;
	//identity map this page (map virt addr directly to phys addr)
	//this is so we can access the page transparently, with its real physical address
	vmm_map(page, page, PAGE_PRESENT | PAGE_WRITE);

	uint32_t magic = 0xDEADBEEF;
	//set the first word of this page
	//note! this is setting using the _virtual_ address, which is mapped to the same physical address
	//so if we read this word back and get the correct info, then the virtual memory manager (paging) is doing what it's supposed to
	*ptr = magic;
	uint32_t word = *((uint32_t*)page);
	ASSERT(word == magic, "%x did not match expected value at page %x", word, magic);

	//double checked it was identity mapped correctly
	uint32_t phys;
	vmm_get_mapping(page, &phys);
	ASSERT(page == phys, "Identity map failed: %x->%x", page, phys);

	//test done
	//unmap this page from VMM and PMM
	vmm_unmap(page);
	pmm_free_page(page);

	printf_info("VMM test passed");
}

void test_printf() {
	printf_info("Testing printf...");
	printf_info("int: %d | hex: %x | char: %c | str: %s | float: %f | %%", 126, 0x14B7, 'q', "test", 3.1415926);
}

void test_time_unique() {
	printf_info("Testing time_unique...");
	for (int i = 0; i < 100; i++) {
		static uint32_t last = 0;
		uint32_t current = time_unique();

		if (last == time_unique()) {
			//we find the number of times this stamp was encountered by
			//reverse engineering the clock slide
			//the slide is the slide stamp minus the real stamp
			printf_err("time_unique failed, stamp %u encountered %u times", time_unique(), time_unique() - time());
			return;
		}
		last = current;
	}
	printf_info("time_unique test passed");
}

void test_crypto() {
	printf_info("Testing SHA256...");
	printf_info("SHA256 test %s", sha256_test() ? "passed":"failed");
	printf_info("Testing AES...");
	printf_info("AES test %s", aes_test() ? "passed":"failed");
}

void test_standard() {
	test_colors();
	test_interrupts();
	test_printf();
	test_time_unique();
	test_vmm();
	test_heap();
	test_crypto();
}
