OUTPUT_FORMAT(elf64-x86-64);
ENTRY(kentry); /* This is just to shut the linker up. */

SECTIONS {
	. = 0x10000000000;
}

