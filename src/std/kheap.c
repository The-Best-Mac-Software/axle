#include "common.h"
#include "kheap.h"
#include <kernel/util/paging/paging.h>
#include "std.h"
#include <std/math.h>

#define PAGE_SIZE 0x1000 /* 4kb page */

//end is defined in linker script
extern uint32_t a_end;
uint32_t placement_address = (uint32_t)&a_end;

extern page_directory_t* kernel_directory;
extern page_directory_t* current_directory;

heap_t* kheap = 0;
static uint32_t used_bytes;

void* kmalloc_int(uint32_t sz, int align, uint32_t* phys) {
	//if the heap already exists, pass through
	if (kheap) {
		void* addr = alloc(sz, (uint8_t)align, kheap);
		if (phys) {
			page_t* page = get_page((uint32_t)addr, 0, kernel_directory);
			*phys = page->frame * PAGE_SIZE + ((uint32_t)addr & 0xFFF);
		}
		return addr;
	}

	//if addr is not already page aligned
	if (align && (placement_address & 0xFFFFF000)) {
		//align it
		placement_address &= 0xFFFFF000;
		placement_address += PAGE_SIZE;
	}
	if (phys) {
		*phys = placement_address;
	}
	uint32_t tmp = placement_address;
	placement_address += sz;

	return (void*)tmp;
}

void* kmalloc_a(uint32_t sz) {
	return kmalloc_int(sz, 1, 0);
}

void* kmalloc_p(uint32_t sz, uint32_t* phys) {
	return kmalloc_int(sz, 0, phys);
}

void* kmalloc_ap(uint32_t sz, uint32_t* phys) {
	return kmalloc_int(sz, 1, phys);
}

void* kmalloc(uint32_t sz) {
	return kmalloc_int(sz, 0, 0);
}

void kfree(void* p) {
	free(p, kheap);
}

static header_t* last_valid_header(heap_t* heap) {
	header_t* curr = heap->start_address;
	while ((curr->next)->magic == HEAP_MAGIC) {
		curr = curr->next;
	}
	return curr;
}

static int32_t find_smallest_hole(uint32_t size, uint8_t align, heap_t* heap) {
	//find smallest hole that will fit
	header_t* curr = heap->start_address;
	while ((curr = curr->next) < heap->end_address) {
		//check if header is valid
		ASSERT(curr->magic == HEAP_MAGIC, "invalid header magic");

		//skip blocks in use
		if (!curr->hole) {
			continue;
		}

		//if user has requested memory be page aligned
		if (align > 0) {
			//page align starting point of header
			uint32_t location = (uint32_t)curr;
			int32_t offset = 0;
			if (((location + sizeof(header_t)) & 0xFFFFF000) != 0) {
				offset = PAGE_SIZE - ((location + sizeof(header_t)) % PAGE_SIZE);
			}

			int32_t hole_size = (int32_t)curr->size - offset;
			//do we still fit?
			if (hole_size >= (int32_t)size) {
				//found good block!
				break;
			}
		}
		//good block!
		else if (curr->size >= size) break;

		curr = curr->next;
	}

	//why did the loop exit?
	//is this an invalid header (did we reach end of list?)
	if (curr->magic != HEAP_MAGIC) {
		//reached end of index and didn't find any holes small enough
		return -1;
	}

	return curr;
}

static bool header_t_less_than(void* a, void* b) {
	return (((header_t*)a)->size < ((header_t*)b)->size);
}

static bool valid_header(heap_t* heap, header_t* header) {
	return (header < heap->end_address && header->magic == HEAP_MAGIC);
}

heap_t* create_heap(uint32_t start, uint32_t end_addr, uint32_t max, uint8_t supervisor, uint8_t readonly) {
	//start and end MUST be page aligned
	ASSERT(start % PAGE_SIZE == 0, "start wasn't page aligned");
	ASSERT(end_addr % PAGE_SIZE == 0, "end_addr wasn't page aligned");

	printf("create_heap(): start %x end %x max %x\n", start, end_addr, max);

	heap_t* heap = (heap_t*)kmalloc(sizeof(heap_t));

	//make sure start is still page aligned)
	if ((start & 0xFFFFF000) != 0) {
		start &= 0xFFFFF000;
		start += PAGE_SIZE;
	}

	//write start, end, and max addresses into heap structure
	heap->start_address = start;
	heap->end_address = end_addr;
	heap->max_address = max;
	heap->supervisor = supervisor;
	heap->readonly = readonly;

	//we start off with one large hole in the index
	//this represents the whole heap at this point
	header_t* hole = (header_t*)start;
	printf("initial hole at %x\n", hole);
	while (1) {}
	hole->magic = HEAP_MAGIC;
	hole->size = end_addr - start;
	hole->hole = 1;
	hole->next = NULL;
	hole->prev = NULL;

	//kheap_print(heap);

	return heap;
}

void expand(uint32_t new_size, heap_t* heap) {
	//sanity check
	ASSERT(new_size > heap->end_address - heap->start_address, "new_size was smaller than heap");
	//get nearest page boundary
	if ((new_size & 0xFFFFF000) != 0) {
		new_size &= 0xFFFFF000;
		new_size += PAGE_SIZE;
	}

	//make sure we're not overreaching ourselves!
	ASSERT(heap->start_address + new_size <= heap->max_address, "heap would exceed max capacity");

	//this *should* always be on a page boundary
	uint32_t old_size = heap->end_address - heap->start_address;
	uint32_t i = old_size;

	//do expansion
	printf("allocated new pages from %x ", heap->start_address + i);
	while (i < new_size) {
		alloc_frame(get_page(heap->start_address + i, 1, kernel_directory), heap->supervisor, !heap->readonly);
		i += PAGE_SIZE;
	}
	heap->end_address = heap->start_address + new_size;
	printf("to %x\n", heap->start_address + i);
}

static uint32_t contract(int32_t new_size, heap_t* heap) {
	//sanity check
	ASSERT((uint32_t)new_size < heap->end_address - heap->start_address, "new_size was larger than heap");

	//get nearest page boundary
	if (new_size & PAGE_SIZE) {
		new_size &= PAGE_SIZE;
		new_size += PAGE_SIZE;
	}

	//don't contract too far
	new_size = MAX(new_size, HEAP_MIN_SIZE);

	int32_t old_size = heap->end_address - heap->start_address;
	int32_t i = old_size - PAGE_SIZE;
	while (new_size < i) {
		free_frame(get_page(heap->start_address + i, 0, kernel_directory));
		i -= PAGE_SIZE;
	}
	heap->end_address = heap->start_address + new_size;
	return new_size;
}

void* alloc(uint32_t size, uint8_t align, heap_t* heap) {
	printf("alloc %x\n", size);
	while (1) {}

	//make sure we take size of header into account
	uint32_t required_size = size + sizeof(header_t);
	//find smallest hole that will fit
	header_t* usable = find_smallest_hole(required_size, align, heap);

	printf("found usable hole %x\n", usable);
	kernel_begin_critical();
	while (1) {}

	if ((uint32_t)usable == -1 || !valid_header(heap, usable)) {
		printf("alloc(): no free hole large enough found\n");
		while (1) {}
		//no free hole large enough was found

		//save some previous data
		uint32_t old_length = heap->end_address - heap->start_address;
		uint32_t old_end_address = heap->end_address;

		//we need to allocate more space
		expand(old_length + required_size, heap);
		uint32_t new_length = heap->end_address - heap->start_address;
		printf("heap expanded by %d bytes\n", new_length - old_length);

		header_t* last = last_valid_header(heap);

		//if we didn't find any headers, add one
		if ((uint32_t)last == -1) {
			header_t* new = (uint32_t)last + last->size;
			last->next = new;
			new->magic = HEAP_MAGIC;
			new->size = heap->end_address - (uint32_t)last + last->size;
			new->hole = 1;
		}
		else {
			last->size += new_length - old_length;
		}

		//we should now have enough space
		//try allocation again
		printf_info("alloc added header, retrying allocation (total size %x)", required_size);
		return alloc(size, align, heap);
	}

	uint32_t orig_hole_size = usable->size;
	uint32_t orig_hole_pos = (uint32_t)usable;

	//check if we should split hole into 2 parts
	//this is only worth it if the new hole's size is greater than the
	//size we need to store the header and footer
	if (orig_hole_size - required_size < sizeof(header_t)) {
		//increase size to size of hole we found
		size += orig_hole_size - required_size;
		required_size = orig_hole_size;
	}

	//if it needs to be page aligned, do it now and
	//make a new hole in front of our block
	if (align && (orig_hole_pos & 0xFFFFF000)) {
		uint32_t new_location = orig_hole_pos + PAGE_SIZE - (orig_hole_pos & 0xFFF) - sizeof(header_t);
		header_t* hole_header = (header_t*)orig_hole_pos;
		hole_header->size = PAGE_SIZE - (orig_hole_pos & 0xFFF) - sizeof(header_t);
		hole_header->magic = HEAP_MAGIC;
		hole_header->hole = 1;

		hole_header->next = usable->next;
		usable->next = hole_header;
		hole_header->prev = usable;
	}
	else {
		usable->prev->next = usable->next;
	}

	//overwrite original header
	header_t* prev = usable->prev;
	header_t* next = usable->next;
	header_t* block_header = (header_t*)orig_hole_pos;
	block_header->magic = HEAP_MAGIC;
	block_header->hole = 0;
	block_header->size = required_size;

	if (orig_hole_size - required_size> 0) {
		header_t* hole = (header_t*)(orig_hole_pos + sizeof(header_t) + size);
		hole->magic = HEAP_MAGIC;
		hole->hole = 1;
		hole->size = orig_hole_size - required_size;

		header_t* last = last_valid_header(kheap);
		last->next = hole;
		hole->prev = last;
	}

	//add this allocation to used memory
	used_bytes += size;

	return (void*)((uint32_t)block_header + sizeof(header_t));
}

void heap_int_test() {
	for (int i = 1; i < 64; i++) {
		char* c = kmalloc(129);

		//get header and footer associated with this pointer
		header_t* header = (header_t*)((uint32_t)c - sizeof(header_t));

		if (header->magic != HEAP_MAGIC) {
			printf_err("heap_int_test(): invalid header magic (got %x)", header->magic);
			kfree(c);
			return;
		}
		kfree(c);
	}
	printf_info("heap_int_test(): test passed normally");
}

void free(void* p, heap_t* heap) {
	if (p == 0) return;

	//get header and footer associated with this pointer
	header_t* header = (header_t*)((uint32_t)p - sizeof(header_t));
	ASSERT(header->magic == HEAP_MAGIC, "invalid header magic in %x (got %x)", p, header->magic);

	//we're about to free this memory, untrack it from used memory
	used_bytes -= header->size;

	//turn this into a hole
	header->hole = 1;

	//attempt merge left
	//if thing to left of us is a footer...
	header_t* prev = header->prev;
	if (prev->magic == HEAP_MAGIC && prev->hole) {
		uint32_t cache_size = header->size; //cache current size

		//rewrite header
		//TODO we don't overwrite the dumped headers memory
		//this could allow someone to figure out heap structure by reading
		prev->next = header->next;
		
		header->size += cache_size; //change size
	}

	//attempt merge right
	//if thing to right of us is a header...
	header_t* next = header->next;
	if (next->magic == HEAP_MAGIC && next->hole) {
		//dump next header and merge it

		header->size += next->next; //increase size to fit merged hole

		header->next = next->next;
	}

	//if footer location is the end address, we can contract
	/*
	if (header + header->size >= heap->end_address) {
		uint32_t old_length = heap->end_address - heap->start_address;
		uint32_t new_length = contract((uint32_t)header - heap->start_address, heap);
		//check how big we'll be after resizing
		if (header->size - (old_length - new_length) > 0) {
			//we still exist, so resize us
			
			header->size -= old_length - new_length;
			footer = (footer_t*)((uint32_t)header + header->size - sizeof(footer_t));
			footer->magic = HEAP_MAGIC;
			footer->header = header;
		}
		else {
			//we no longer exist
			//remove us from index
			uint32_t iterator = 0;
			while ((iterator < heap->index->size) && (array_o_lookup(heap->index, iterator) != (void*)test_header)) {
				iterator++;
			}

			//if we didn't find ourselves, we have nothing to remove
			if (iterator < heap->index->size) {
				array_o_remove(heap->index, iterator);
			}
		}
	}
	*/
}

uint32_t used_mem() {
	return used_bytes;
}

void kheap_print(heap_t* heap) {
	//first header is at beginning of heap memory
	header_t* header = (header_t*)heap->start_address;
	if (!valid_header(heap, header)) {
		printf("initial header had invalid magic %x\n", header->magic);
		return;
	}
	while (valid_header(heap, header)) {
		printf("valid header %x\n", (uint32_t)header);
		if (header->magic != HEAP_MAGIC) {
			printf("%x block had invalid heap magic\n", &header);
			return;
		}

		printf("block @ %x ", header);
		if (header->hole) {
			printf("free ");
		}
		else {
			printf("used ");
		}

		printf("size: %x bytes\n", header->size);

		//go to next header
		header = header->next;
	}
}
