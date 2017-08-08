/*
 Copyright <2017> <Scaleable and Concurrent Systems Lab; 
                   Thayer School of Engineering at Dartmouth College>

 Permission is hereby granted, free of charge, to any person obtaining a copy 
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights 
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
 copies of the Software, and to permit persons to whom the Software is 
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
*/

#pragma once
/* elf.h -- stuff for the ELF reader/writer.
 * The definitions in this file are (mostly) taken from the ELF spec.
 * 
 * We don't define the note section in this file because there's no way to do 
 * so statically.
*/

#include <stdint.h>

/* ELF types.
 * Note that in ELF, all data types must also be aligned on their size. */
typedef uint32_t Elf32_Addr;
typedef uint32_t Elf32_Off;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Word;
typedef int32_t Elf32_Sword;
typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t Elf64_Sxword;

/* File header. */
struct Elf32_Ehdr {
	unsigned char e_ident[16];     /* ELF identification */
	Elf32_Half    e_type;          /* Object file type */
	Elf32_Half    e_machine;       /* Machine type */
	Elf32_Word    e_version;       /* Object file version */
	Elf32_Addr    e_entry;         /* Entry point address */
	Elf32_Off     e_phoff;         /* Program header offset */
	Elf32_Off     e_shoff;         /* Section header offset */
	Elf32_Word    e_flags;         /* Processor-specific flags */
	Elf32_Half    e_ehsize;        /* ELF header size */
	Elf32_Half    e_phentsize;     /* Size of program header entry */
	Elf32_Half    e_phnum;         /* Number of program header entries */
	Elf32_Half    e_shentsize;     /* Size of section header entry */
	Elf32_Half    e_shnum;         /* Number of section header entries */
	Elf32_Half    e_shstrndx;      /* Section name string table index */
};

struct Elf64_Ehdr {
	unsigned char e_ident[16];       /* ELF identification */
	Elf64_Half    e_type;            /* Object file type */
	Elf64_Half    e_machine;         /* Machine type */
	Elf64_Word    e_version;         /* Object file version */
	Elf64_Addr    e_entry;           /* Entry point address */
	Elf64_Off     e_phoff;           /* Program header offset */
	Elf64_Off     e_shoff;           /* Section header offset */
	Elf64_Word    e_flags;           /* Processor-specific flags */
	Elf64_Half    e_ehsize;          /* ELF header size */
	Elf64_Half    e_phentsize;       /* Size of program header entry */
	Elf64_Half    e_phnum;           /* Number of program header entries */
	Elf64_Half    e_shentsize;       /* Size of section header entry */
	Elf64_Half    e_shnum;           /* Number of section header entries */
	Elf64_Half    e_shstrndx;        /* Section name string table index */
};

/* Enumeration of bytes in the e_ident string. */
#define EL_MAG0  0                           /* File ID (magic number) */
#define EL_MAG1  1                           /* File ID (magic number) */
#define EL_MAG2  2                           /* File ID (magic number) */
#define EL_MAG3  3                           /* File ID (magic number) */
#define EL_CLASS 4                           /* File class */
#define EL_DATA  5                           /* Data encoding */
#define EL_VERSION 6                         /* File version */
#define EL_OSABI   7                         /* OS/ABI identification */
#define EL_ABIVERSION 8                      /* ABI version */
#define EL_PAD    9                          /* Start of padding bytes */
#define EL_NIDENT 16                         /* Size of e_ident[] */

/* Enumeration of object file classes, e_ident[EL_CLASS] */
#define ELFCLASS32 1                         /* 32-bit objects */
#define ELFCLASS64 2                         /* 64-bit objects */

/* Enumeration of data encodings, e_ident[EL_DATA] */
#define ELFDATA2LSB 1                        /* Little-endian */
#define ELFDATA2MSB 2                        /* Big-endian */

/* Enumeration of operating system and ABI identifiers, e_ident[EL_OSABI] */
#define ELFOSABI_SYSV 0                      /* System V ABI */
#define ELFOSABI_HPUX 1                      /* HP-UX operating system */
#define ELFOSABI_STANDALONE 255              /* Embedded system */

/* Enumeration of object file types, e_type */
#define ET_NONE 0                            /* No file type */
#define ET_REL  1                            /* Relocatable object file */
#define ET_EXEC 2                            /* Executable file */
#define ET_DYN  3                            /* Shared object file */
#define ET_CORE 4                            /* Core file */
#define ET_LOOS 0xFE00                       /* Environment-specific use */
#define ET_HIOS 0xFEFF                       /* Environment-specific use */
#define ET_LOPROC 0xFF00                     /* Processor-specific use */
#define ET_HIPROC 0xFFFF                     /* Processor-specific use */

/* Special section indices */
#define SHN_UNDEF  0              /* Undefined/meaningless section */
#define SHN_LOPROC 0xFF00         /* Processor-specific use */
#define SHN_HIPROC 0xFF1F         /* Processor-specific use */
#define SHN_LOOS   0xFF20         /* Environment-specific use */
#define SHN_HIOS   0xFF3F         /* Environment-specific use */
#define SHN_ABS    0xFFF1         /* Corresponding ref is an absolute value */
#define SHN_COMMON                /* Symbol declared as a common block */

/* Section header. */
struct Elf32_Shdr {
	Elf32_Word sh_name;       /* Section name */
	Elf32_Word sh_type;       /* Section type */
	Elf32_Word sh_flags;      /* Section attributes */
	Elf32_Addr sh_addr;       /* Virtual address in memory */
	Elf32_Off  sh_offset;     /* Offset in file */
	Elf32_Word sh_size;       /* Size of section */
	Elf32_Word sh_link;       /* Link to other section */
	Elf32_Word sh_info;       /* Miscellaneous information */
	Elf32_Word sh_addralign;  /* Address alignment boundary */
	Elf32_Word sh_entsize;    /* Size of entries, if section has a table */
};

struct Elf64_Shdr {
	Elf64_Word  sh_name;      /* Section name */
	Elf64_Word  sh_type;      /* Section type */
	Elf64_Xword sh_flags;     /* Section attributes */
	Elf64_Addr  sh_addr;      /* Virtual address in memory */
	Elf64_Off   sh_offset;    /* Offset in file */
	Elf64_Xword sh_size;      /* Size of section */
	Elf64_Word  sh_link;      /* Link to other section */
	Elf64_Word  sh_info;      /* Miscellaneous information */
	Elf64_Xword sh_addralign; /* Address alignment boundary */
	Elf64_Xword sh_entsize;   /* Size of entries, if section has a table */
};

/* Enumeration of values for sh_type in the preceding structure.
 * These tell what the section header contains.
*/
#define SHT_NULL     0       /* Contains unused section header */
#define SHT_PROGBITS 1       /* Contains information defined by the program */
#define SHT_SYMTAB   2       /* Contains a linker symbol table */
#define SHT_STRTAB   3       /* Contains a string table */
#define SHT_RELA     4       /* Contains "Rela" type relocation entries */
#define SHT_HASH     5       /* Contains a symbol hash table */
#define SHT_DYNAMIC  6       /* Contains dynamic linking tables */
#define SHT_NOTE     7       /* Contains note information */
#define SHT_NOBITS   8       /* Contains uninitialized space; does not
                              * occupy any space in the file */
#define SHT_REL      9       /* Contains "Rel" type relocation entries */
#define SHT_SHLIB   10       /* Reserved */
#define SHT_DYNSYM  11       /* Contains a dynamic loader symbol table */
#define SHT_LOOS    0x60000000              /* Environment-specific use */
#define SHT_HIOS    0x6FFFFFFF              /* Environment-specific use */
#define SHT_LOPROC  0x70000000              /* Processor-specific use */
#define SHT_HIPROC  0x7FFFFFFF              /* Processor-specific use */
#define SHT_X86_64_UNWIND 0x70000001        /* Unwind function table entries */

/* Enumeration of values for sh_flags in the preceding structure.
 * These can all be OR-ed together.
*/
#define SHF_WRITE     0x1                 /* Section contains writable data */
#define SHF_ALLOC     0x2                 /* Section is allocated in memory
                                           * image of program */
#define SHF_EXECINSTR 0x4                 /* Section contains executable
                                           * instructions */
#define SHF_TLS       0x400               /* Contains thread-local storage */
#define SHF_MASKOS    0x0F000000          /* Environment-specific use */
#define SHF_MASKPROC  0xF0000000          /* Processor-specific use */
#define SHF_X86_64_LARGE 0x1000000        /* Section > 2GB */

/* Symbol table structure.
 * Note that the first table entry is reserved and must  be all zeroes. */
#define STN_UNDEF 0                        /* Refers to first symtab entry */
struct Elf32_Sym {
	Elf32_Word    st_name;             /* name's index in string table */
	unsigned char st_info;             /* Type and Binding attributes;
	                                    * binding in the high-order 4 bits
	                                    * and type in the lower 4 bits. */
	unsigned char st_other;            /* Reserved */
	Elf32_Half    st_shndx;            /* Section table index */
	Elf32_Addr    st_value;            /* Symbol value */
	Elf32_Word    st_size;             /* Size of object (e.g., common) */
};

struct Elf64_Sym {
	Elf64_Word    st_name;              /* name's index in string table */
	unsigned char st_info;              /* Type and Binding attributes;
	                                     * binding in the high-order 4 bits
	                                     * and type in the lower 4 bits. */
	unsigned char st_other;             /* Reserved */
	Elf64_Half    st_shndx;             /* Section table index */
	Elf64_Addr    st_value;             /* Symbol value */
	Elf64_Xword   st_size;              /* Size of object (e.g., common) */
};

#define ELF64_ST_TYPE(info)          ((info) & 0xf)
#define ELF64_ST_BIND(info)          ((info) >> 4)

/* Enumeration of symbol bindings */
#define STB_LOCAL  0                 /* Not visible outside the object file */
#define STB_GLOBAL 1                 /* Global symbol, visible to all object
                                      * files */
#define STB_WEAK   2                 /* Global scope, but with lower
                                      * precedence than global symbols*/
#define STB_LOOS   10                /* Environment-specific use */
#define STB_HIOS   12                /* Environment-specific use */
#define STB_LOPROC 13                /* Processor-specific use */
#define STB_HIPROC 15                /* Processor-specific use */

/* Enumeration of symbol types */
#define STT_NOTYPE  0                /* No type specified (e.g., an
                                      * absolute symbol) */
#define STT_OBJECT  1                /* Data object */
#define STT_FUNC    2                /* Function entry point */
#define STT_SECTION 3                /* Symbol associated with a section */
#define STT_FILE    4                /* Source file associated with the
                                      * object file */
#define STT_LOOS    10               /* Environment-specific use */
#define STT_HIOS    12               /* Environment-specific use */
#define STT_LOPROC  13               /* Processor-specific use */
#define STT_HIPROC  15               /* Processor-specific use */

/* Structures and definitions for relocations */
struct Elf32_Rel {
	Elf32_Addr r_offset;          /* Address of reference */
	Elf32_Word r_info;            /* Symbol index and type of relocation */
};

struct Elf64_Rel {
	Elf64_Addr  r_offset;        /* Address of reference */
	Elf64_Xword r_info;          /* Symbol index and type of relocation */
};

struct Elf32_Rela {
	Elf32_Addr  r_offset;         /* Address of reference */
	Elf32_Word  r_info;           /* Symbol index and type of relocation */
	Elf32_Sword r_addend;         /* Constant part of expression */
};

struct Elf64_Rela {
	Elf64_Addr   r_offset;        /* Address of reference */
	Elf64_Xword  r_info;          /* Symbol index and type of relocation */
	Elf64_Sxword r_addend;        /* Constant part of expression */
};

/* Macros to extract information from r_info */
#define ELF32_R_SYM(i) ((i) >> 8)
#define ELF32_R_TYPE(i) ((i) & 0xFF)
#define ELF32_R_INFO(s,t) (((s) << 8) + ((t) & 0xff))
#define ELF64_R_SYM(i) ((i) >> 32)
#define ELF64_R_TYPE(i) ((i) & 0xFFFFFFFFL)
#define ELF64_R_INFO(s,t) (((s) << 32) + ((t) & 0xFFFFFFFFL))

/* Relocation types */
#define R_X86_64_NONE 0
#define R_X86_64_64 1
#define R_X86_64_PC32 2
#define R_X86_64_GOT32 3
#define R_X86_64_PLT32 4
#define R_X86_64_COPY 5
#define R_X86_64_GLOB_DAT 6
#define R_X86_64_JUMP_SLOT 7
#define R_X86_64_RELATIVE 8
#define R_X86_64_GOTPCREL 9
#define R_X86_64_32 10
#define R_X86_64_32S 11
#define R_X86_64_16 12
#define R_X86_64_PC16 13
#define R_X86_64_8 14
#define R_X86_64_PC8 15
#define R_X86_64_DTPMOD64 16
#define R_X86_64_DTPOFF64 17
#define R_X86_64_TPOFF64 18
#define R_X86_64_TLSGD 19
#define R_X86_64_TLSLD 20
#define R_X86_64_DTPOFF32 21
#define R_X86_64_GOTTPOFF 22
#define R_X86_64_TPOFF32 23
#define R_X86_64_PC64 24
#define R_X86_64_GOTOFF64 25
#define R_X86_64_GOTPC32 26
#define R_X86_64_SIZE32 32
#define R_X86_64_SIZE64 33
#define R_X86_64_GOTPC32_TLSDESC 34
#define R_X86_64_TLSDESC_CALL 35
#define R_X86_64_TLSDESC 36

/* The program header (segment) table. */
struct Elf32_Phdr {
	Elf32_Word p_type;                      /* Type of segment */
	Elf32_Off  p_offset;                    /* Offset in file */
	Elf32_Addr p_vaddr;                     /* Virtual address in memory */
	Elf32_Addr p_paddr;                     /* Reserved */
	Elf32_Word p_filesz;                    /* Size of segment in file */
	Elf32_Word p_memsz;                     /* Size of segment in memory */
	Elf32_Word p_flags;                     /* Segment attributes */
	Elf32_Word p_align;                     /* Alignment of segment */
};

/* The program header (segment) table. */
struct Elf64_Phdr {
	Elf64_Word  p_type;                     /* Type of segment */
	Elf64_Word  p_flags;                    /* Segment attributes */
	Elf64_Off   p_offset;                   /* Offset in file */
	Elf64_Addr  p_vaddr;                    /* Virtual address in memory */
	Elf64_Addr  p_paddr;                    /* Reserved */
	Elf64_Xword p_filesz;                   /* Size of segment in file */
	Elf64_Xword p_memsz;                    /* Size of segment in memory */
	Elf64_Xword p_align;                    /* Alignment of segment */
};

/* Enumeration of values for p_type in the preceding structure. */
#define PT_NULL    0                        /* Unused entry */
#define PT_LOAD    1                        /* Loadable segment */
#define PT_DYNAMIC 2                        /* Dynamic linking tables */
#define PT_INTERP  3                        /* Program interpreter path name */
#define PT_NOTE    4                        /* Note sections */
#define PT_SHLIB   5                        /* reserved */
#define PT_PHDR    6                        /* Program header table */
#define PT_TLS     7                        /* Thread-local storage */
#define PT_LOOS    0x60000000               /* Environment-specific use */
#define PT_HIOS    0x6FFFFFFF               /* Environment-specific use */
#define PT_LOPROC  0x70000000               /* Processor-specific use */
#define PT_HIPROC  0x7FFFFFFF               /* Processor-specific use */
#define PT_GNU_EH_FRAME 0x6474E550
#define PT_GNU_RELRO 0x6474E552
#define PT_SUNW_EH_FRAME 0x6474E550
#define PT_SUNW_UNWIND 0x6474E550

/* Enumeration of values for p_flags in the preceding structure.
 * These can all be OR-ed together.
*/
#define PF_X 0x1                             /* Execute permission */
#define PF_W 0x2                             /* Write permission */
#define PF_R 0x4                             /* Read permission */
#define PF_MASKOS 0x00FF0000                 /* Environment-specific use */
#define PF_MASKPROC 0xFF000000               /* Processor-specific use */


/* Dynamic table */
struct Elf32_Dyn {
	Elf32_Sword d_tag;
	union {
		Elf32_Sword d_val;
		Elf32_Addr d_ptr;
	} d_un;
};

struct Elf64_Dyn {
	Elf64_Sxword d_tag;
	union {
		Elf64_Xword d_val;
		Elf64_Addr d_ptr;
	} d_un;
};
extern struct Elf64_Dyn _DYNAMIC[];

/* Enumeration of types for d_tag in the preceding structure.
 * Each type uses the d_un union in a different way, so be sure to check the 
 * ELF documentation for information as to what each of these means.
*/
#define DT_NULL 0
#define DT_NEEDED 1
#define DT_PLTRELSZ 2
#define DT_PLTGOT 3
#define DT_HASH 4
#define DT_STRTAB 5
#define DT_SYMTAB 6
#define DT_RELA 7
#define DT_RELASZ 8
#define DT_RELAENT 9
#define ST_STRSZ 10
#define DT_SYMENT 11
#define DT_INIT 12
#define DT_FINI 13
#define DT_SONAME 14
#define DT_RPATH 15
#define DT_SYMBOLIC 16
#define DT_REL 17
#define DT_RELSZ 18
#define DT_RELENT 19
#define DT_PLTREL 20
#define DT_DEBUG 21
#define DT_TEXTREL 22
#define DT_JMPREL 23
#define DT_BIND_NOW 24
#define DT_INIT_ARRAY 25
#define DT_FINI_ARRAY 26
#define DT_INIT_ARRAYSZ 27
#define DT_FINI_ARRAYSZ 28
#define DT_LOOS 0x60000000
#define DT_HIOS 0x6FFFFFFF
#define DT_LOPROC 0x70000000
#define DT_HIPROC 0x7FFFFFFF

/* Macro for determining if an ELF section lies in a segment. There are several
 * complex rules missing for this that the (unfortunately GPLv3 licensed)
 * version from GNU Binutils covers, but I do not believe they apply to us. */
#define ELF_SECTION_IN_SEGMENT(sec, seg)                                \
	(                                                                   \
	/* Basic check: Non-NOBITS sections must have *file* (not			\
	 * memory) offsets within the segment. */							\
		((sec)->sh_type == SHT_NOBITS									\
		 || (((sec)->sh_offset >= (seg)->p_offset)						\
		     && (((sec)->sh_offset + (sec)->sh_size) <=					\
		         ((seg)->p_offset + (seg)->p_filesz))))					\
	/* Sections flagged with Alloc must also have their vmem addresses
	 * fall	within the given segment. */								\
		&& (((sec)->sh_flags & SHF_ALLOC) == 0							\
		    || (((sec)->sh_addr >= (seg)->p_vaddr)						\
		        && (((sec)->sh_addr + (sec)->sh_size) <=				\
		            ((seg)->p_vaddr + (seg)->p_memsz)))))

/* Type abstraction so we can freely use 32-bit or 64-bit ELF structures 
 *  without duplicating code. This is all done at compile time, we have to 
 * compile with either 32-bit or 64-bit ELF (can't mix with this). */
#define Elf_Ehdr Elf64_Ehdr
#define Elf_Shdr Elf64_Shdr
#define Elf_Phdr Elf64_Phdr
#define Elf_Sym Elf64_Sym
#define Elf_Rel Elf64_Rel
#define Elf_Rela Elf64_Rela
#define Elf_Dyn Elf64_Dyn
#define Elf_Addr Elf64_Addr
