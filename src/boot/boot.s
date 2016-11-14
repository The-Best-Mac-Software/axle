MBOOT_PAGE_ALIGN	equ 1<<0	; load kernel and modules on page boundary
MBOOT_MEM_INFO		equ 1<<1	; provide kernel. with memory info
MBOOT_HEADER_MAGIC	equ 0x1BADB002	; multiboot magic value
MBOOT_HEADER_FLAGS	equ MBOOT_PAGE_ALIGN | MBOOT_MEM_INFO
MBOOT_CHECKSUM		equ -(MBOOT_HEADER_MAGIC + MBOOT_HEADER_FLAGS)

[BITS 32]

[GLOBAL mboot]
[SECTION .mboot]
mboot:
	dd MBOOT_HEADER_MAGIC	; header value for GRUB
	dd MBOOT_HEADER_FLAGS	; grub settings
	dd MBOOT_CHECKSUM	; ensure above values are correct

[SECTION .text]
[GLOBAL start]			; entry point
[EXTERN kernel_main]		; C entry point
start:
	;initial stack
	mov esp, stack_space;
	push esp
	; load multiboot information
	push ebx
	mov ebp, 0

	; execute kernel
	cli
	call kernel_main
	hlt
	jmp $			; enter infinite loop so processor doesn't
				; try executing junk values in memory
.end:


; set size of the _start symbol to the current location '.' minus its start
; .size _start, . -_start
[SECTION .bss]
RESB 8192
stack_space:
