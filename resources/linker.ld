OUTPUT_FORMAT(elf32-i386)
ENTRY(_loader)

phys = 0x100000;
offset = 0xC0000000;
virt = offset + phys;

SECTIONS {
	. = 0xC0100000;

    .text : AT(ADDR(.text) - 0xC0000000) {
		a_code = .;
		*(.text)
        *(.rodata*)
    }

    .data ALIGN(0x1000) : AT(ADDR(.data) - 0xC0000000) {
		a_data = .;
        *(.data)
    }

    .bss : AT(ADDR(.bss) - offset) {
		a_bss = .;
		*(COMMON)
        *(.bss)
    }

	a_end = .;
}
