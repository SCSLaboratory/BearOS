#pragma once

#pragma once
#define _IN_ASM

#define ASM_ENTRY_ALIGN 16

/* Entry provides a deprecated procedure entry code for calling assembler. */
#define ENTRY(x)                                \
    .text;                                      \
    .align ASM_ENTRY_ALIGN;                     \
    .globl x;                                   \
    .type x, @function;                         \
x:

/* Data entry provides a method for inserting global variables in assembler. */
#define DATA_ENTRY(x)                           \
    .data;                                      \
    .align ASM_ENTRY_ALIGN;                     \
    .globl x;                                   \
    .type x, @object;                           \
x:

/* SET_SIZE trails a function or data element and sets the size for the ELF
 * symbol table. */
#define SET_SIZE(x)                             \
	.size	x, [.-x]

/* Function call using relocations. We have to do this to force the assembler to
 * give us the type of relocations that we want, so it works w/ diversity. This
 * is only required because the assembler does not want to generate R_X86_64_64
 * relocations. This uses %rbx, and it DOES NOT SAVE ITS VALUE. If you need to
 * save %rbx, SAVE IT BEFORE CALLING THIS MACRO. */
#define RELCALL(symbol)                         \
	movabs $0x0,%rbx;                           \
	.reloc .-8, R_X86_64_64, symbol;            \
	callq *%rbx
