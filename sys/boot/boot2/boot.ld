/* Linker script for the post-MBR boot code. It's linked as a binary
 * file, just flat, no header or anything like that. The entry point
 * that the MBR code should call is at the very start of the file,
 * which means it's at the very top of the memory where this is
 * loaded. The rest of the code comes right after, then the data.
*/
OUTPUT_FORMAT(binary);
ENTRY(start); /* This is just to shut the linker up. */

SECTIONS {
	. = 0x8000;
	.startpoint : { *(startpoint) } =0x90
	.text ALIGN(0) : SUBALIGN(0) { *(.text) } =0x90
	.bss : { *(.bss) } =0x90
	.data : { *(.data) } =0x90
	/* strings, which in clang is a strange section name */
	.rodata : { *(.rodata*) } = 0x90
	/DISCARD/ : { *(*) }
}
