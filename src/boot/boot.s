global _loader
extern kernel_main

MBOOT_PAGE_ALIGN	equ 1<<0	; load kernel and modules on page boundary
MBOOT_MEM_INFO		equ 1<<1	; provide memory map
MBOOT_HEADER_FLAGS	equ MBOOT_PAGE_ALIGN | MBOOT_MEM_INFO
MBOOT_HEADER_MAGIC	equ 0x1BADB002	; multiboot magic value
MBOOT_CHECKSUM		equ -(MBOOT_HEADER_MAGIC + MBOOT_HEADER_FLAGS)

KERNEL_VIRTUAL_BASE equ 0xC0000000
KERNEL_PAGE_NUMBER equ (KERNEL_VIRTUAL_BASE >> 22) ;page directory entry of kernel's 4mb pte

section .data
align 0x1000
boot_page_dir:
	; this page dir identity maps first 4mb of physical address space 
	; bit 7: 4mb pages
	; bit 1: rw
	; bit 0: present
	dd 0x00000083
	times (KERNEL_PAGE_NUMBER - 1) dd 0 ; pages before kernel space
	dd 0x00000083
	times (1024 - KERNEL_PAGE_NUMBER - 1) dd 0 ; pages after kernel image

section .text
align 4
	dd MBOOT_HEADER_MAGIC
	dd MBOOT_HEADER_FLAGS
	dd MBOOT_CHECKSUM

;initial stack space (16k)
STACKSIZE equ 0x4000

_loader:
	; must use absolute addresses until paging enabled
	mov ecx, (boot_page_dir - KERNEL_VIRTUAL_BASE)
	mov cr3, ecx ; set page dir

	mov ecx, cr4
	or ecx, 0x10 ; 4mb pages!!!
	mov cr4, ecx

	mov ecx, cr0
	or ecx, 0x80000000 ; enable paging
	mov cr0, ecx
	
	; set eip to addr in kernel space
	; before this, eip hold physical addr of instr (around 1MB)
	; do long jump to correct virtual address of higher_half
	; which is approx. 0xC0100000
	lea ecx, [higher_half] 
	jmp ecx ; must be an absolute jump

higher_half:
	; unmap identity-mapped first 4mb
	; no longer needed, we're in higher half
	mov dword [boot_page_dir], 0
	invlpg [0]

	mov esp, stacktop ; set up initial stack
	push esp
	push eax ; multiboot magic number
	add ebx, KERNEL_VIRTUAL_BASE
	push ebx
	
	cli 
	call kernel_main
	cli
	hlt

section .bss
align 32
stack:
	resb STACKSIZE
stacktop:
