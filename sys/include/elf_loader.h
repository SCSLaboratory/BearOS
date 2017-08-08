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

/* elf_loader.h -- declarations for the ELF loader. */
#pragma once

#include <elf.h>
#include <list.h>
#include <kqueue.h>

/* bad_region: specify a region of memory that should be declared "off-limits"
 * for whatever reason (usually because something else has to be there. Used
 * in ELF loading to make sure we don't accidentally overwrite something we
 * need to be keeping around. */
struct bad_region {
  uint64_t start;
  uint64_t end;

  /* Sometimes use this as a linked list. */
  struct list_node list;
};

/* Because there are two possible types of relocations, we'll use a union so we
 * don't have to duplicate everything. */
union reloc {
  struct Elf_Rel rel;
  struct Elf_Rela rela;
};

/* To avoid the needless rereading of structures from the disk, as well as 
   making sure that the loader is reentrant for a multiprocessing system, we 
   use the following structure. */
struct elf_ctx {
  struct Elf_Ehdr file_header;
  int file_header_loaded;
  struct Elf_Phdr *program_headers;
  int program_headers_loaded;
  struct Elf_Shdr *section_headers;
  int section_headers_loaded;
  uint8_t *section_strtab;
  int section_strtab_loaded;
  uint8_t *strtab;
  int strtab_loaded;
  struct Elf_Sym *symtab;
  int num_syms;

  /* Diversity units; aka, code blocks we can relocate. */
  struct list_head diversity_units;
  /* Map from a section header to its associated diversity unit. The pointer
   * goes to NULL if a header has no associated diversity unit (i.e., it isn't
   * a text section). */
  struct diversity_unit **unitmap;
  int num_diversity_units;
  int diversity_units_processed;

  /* Static units, if we have any. Type is a queue. */
  void *static_units;

  /* Concatenated list of all relocations. These are pointers to the actual
   * data, which is owned by the individual diversity units. */
  struct Elf_Rel **rels;
  struct Elf_Rela **relas;
  size_t num_rels;
  size_t num_relas;
  int relocations_processed;

#ifdef REPLICATION
  /* All shdrs for diversity units that are replicas of other units. */
  struct Elf_Shdr *replicated_shdrs;
  int replication_done;
#endif

  /* File I/O stuff. */
  void *file_ctx;
  void (*readfunc)(void *, void *, size_t);
  void (*seekfunc)(void *, size_t);
  int (*errorfunc)(void *);
};

/* Since we are loading binaries over the network, we need to put some ELF
 * information into a temporary structure. No diversity/replication information
 * is compromised though
 */
struct elf_ctx_user {
  struct Elf_Ehdr file_header;
  struct Elf_Phdr *program_headers;
  struct Elf_Shdr *section_headers;
  uint8_t *section_strtab;
  uint8_t *strtab;
  struct Elf_Sym *symtab;

  uint64_t *pt_load_segment;
  int size_pt_load_segment;

  int size_phdr;
  int size_shdr;
  int size_section_strtab;
  int size_strtab;
  int size_symtab;
};

/* Get section name from hdr's name field. */
#define ELF_SECNAME(hdr, strtab) ((char *)((strtab)+((hdr).sh_name)))
/* Get symbol name from sym's name field. */
#define ELF_SYMNAME(sym, strtab) ((char *)((strtab)+((sym).st_name)))

struct diversity_unit {
  struct Elf_Shdr *hdr;                  /* Header corresponding to unit. */
  struct Elf_Sym **syms;                 /* Symbols corresponding to unit. */
  size_t num_syms;

  /* Relocation stuff. Generally, a unit will only have one of a .rel or .rela
   * section and relocations, not both. Still, we will make sure we keep track
   * of both in the rare chance that both exist. There should be AT LEAST one
   * of them; having none means no relocations are contained in the unit. */
  struct Elf_Shdr *rel_hdr;              /* Corresponding .rel section */
  struct Elf_Shdr *rela_hdr;             /* Corresponding .rela section */
  struct Elf_Rel *rels;                  /* Rels in this section/unit. */
  struct Elf_Rela *relas;                /* Relas in this section/unit. */
  size_t num_rels;                         /* # rels in this section/unit. */
  size_t num_relas;                        /* # relas in this section/unit. */

  /* EXTERNAL relocations. These are the relocations for OUR OWN symbol(s)
   * that we need to update whenever we perform a diversification action.
   * These relocations would all have ELF64_R_SYM(r_info) corresponding to one
   * of our own symbols. This is an array of linked lists, total number equal
   * to num_syms. */
  struct list_head external_relocs;

  /* Information about where the diversity unit resides currently in memory.
   * We can probably strictly derive this information from other sources, but
   * its useful to just know when doing (re)diversification. */
  uint64_t addr;
  size_t memsz;

#ifdef REPLICATION
  /* Only text sections are replicated. */
  int is_replicated;
  /* Are we a replica? */
  int replica;
  /* Only used for the original unit. */
  struct diversity_unit *replicas[REPLICATION];
#endif

  struct list_node list;                   /* Linked list structure. */

  uint64_t offset;
  int sign;

  /* Dynamically allocated lists: syms, rels, relas.
   * The contents of rels and relas are owned by this structure. */
};

/* Some units should not be relocated or diversified -- they need to be copied
 * exactly is-is into any new memory spaces. These are represented with this
 * structure. */
struct static_unit {
  struct Elf_SHdr *shdr;          /* Corresponding ELF header, if it exists */
  uint64_t addr;
  uint64_t size;
  enum {
    STATIC_ALLOCATED = 0,
    STATIC_UNALLOCATED = 1
  } type;
};

/* Relocation used in the external_relocs list in diversity_unit. A list member
 * contains a pointer to the relocation and an enumeration declaring the
 * relocation type. */
struct external_reloc {
  union {
    struct Elf_Rel *rel;
    struct Elf_Rela *rela;
  };
  enum {
    REL = 0,
    RELA = 1
  } type;

  struct list_node list;
};

/* Test to see if a diversity unit contains the given address.
 * Usage: if(UNIT_CONTAINS(unit, address) { do stuff }
 */
#define UNIT_CONTAINS(unit, ptr) ((unit)->addr <= (ptr) && (ptr) < (unit)->addr + (unit)->memsz)

/* Allocate and free an elf_ctx. */
struct elf_ctx *alloc_elf_ctx(void (*readfunc)(void *, void *, size_t),
                              void (*seekfunc)(void *, size_t),
                              int (*errorfunc)(void *));
/* Be sure to close the file_ctx inside first; this destructor does not do so. */
void free_elf_ctx(struct elf_ctx *ctx);

/* Likewise for diversity units. */
struct diversity_unit *alloc_diversity_unit(struct Elf_Shdr *shdr);
void free_diversity_unit(struct diversity_unit *unit);

/* A function passed to the loader by the kernel or hypervisor, that will add a
 * page to a given (v)proc's address space. The first argument is the (v)proc
 * structure, which of course depends on whether or not we're in the hypervisor
 * or kernel. The second argument is the virtual address; the third is the
 * physical address (if provided, otherwise MEM_FAIL); the third, fourth, and
 * fifth are read, write, and exec privileges, respectively.
 *
 * On success, this function returns a pointer to the virtual address of the
 * page THAT IS CURRENTLY ADDRESSIBLE (i.e., usually meaning "in kernel space")
 * On failure, it should return with all bits set.
 */
typedef void *(*elf_page_adder)(void *, uint64_t, uint64_t, int, int, int);

/* elf_load_file: Loads a file into the given pointer of memory (usually 0x0),
 *                with some limitations.
 * Definition of parameters:
 *   proc:     Pointer to the (v)proc structure we are loading.
 *
 *   ctx:      Pointer to the elf context we are loading.
 *
 *   add_page: Function to be called when new page must be allocated and added
 *             to proc's address space.
 *
 *   brs:      An array of regions that the loader should NOT overwrite if the
 *             ELF section would put memory there. If it does, error out of the
 *             function instead of continue.
 *
 *  num_brs:   How many elements are in the brs array.
 *
 *  RETURNS: The entry point to the loaded binary will be returned on success,
 *           with MEM_FAIL returned on failure.
 */
uint64_t elf_load_file(
#ifndef BOOTLOADER
		       void *proc, 
#endif
		       struct elf_ctx *ctx
#ifdef HYPV 
		       , struct bad_region *brs, size_t num_brs
#endif
		       );

int reloc_size(int reloctype);
int elf_load_metadata(struct elf_ctx *ctx);
char *read_string_table(void *file_ctx);
char *read_section_table(void *file_ctx);

char * search_symbol_name(uint64_t address, char *file_name);

uint64_t* symbol_addr_list(void *file_ctx, int *sym_count);
char * get_section_name( uint64_t offset, char * string_table);
char *get_symbol_name( void *file_ctx, uint64_t address, char* string_table);
uint64_t get_symbol_addr(void* file_ctx, char* sym_name, char * string_table);
