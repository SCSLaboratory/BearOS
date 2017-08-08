OUTPUT_FORMAT(elf64-x86-64);
SEARCH_DIR (/home/martin/bear.v3/build.x86_64/lib)
STARTUP(crt0.o)
ENTRY(_start);

SECTIONS
{
	. = 0x40000000000;

	/* Additional symbols */
	sbrk_end = 0x400F0000000;
}

