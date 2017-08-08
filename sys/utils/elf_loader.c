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

/*
 * elf_loader.c -- ELF loader for the hypervisor & kernel.
 *
 * Bear Requirements for ELF files:
 *     1. All segments MUST be page aligned.
 *     2. Segments MUST NOT overlap each other.
 *
 * TODO: All verification.
 *         Make sure that no symbols have SHN_UNDEF.
 *         
 * All the various list-loops in the loading process could be done much
 * more efficiently by looping once and saving the interesting nodes
 */
#include <constants.h>
#include <elf.h>
#include <elf_loader.h>
#include <khash.h>
#include <kstdio.h>
#include <kstring.h>
#include <memory.h>
#include <kqueue.h>
#include <container_of.h>
#include <kmalloc.h>
#include <file_abstraction.h>
#ifdef HYPV
//#include <hypv.h> WHYYYYYYY?
#else
#include <kvmem.h>
#endif
#include <proc.h>

#ifdef KERNEL 
#include <procman.h>
#endif

#include <check_type.h>
#include <fatfs.h>


struct elf_ctx *alloc_elf_ctx(void (*readfunc)(void *, void *, size_t),
                              void (*seekfunc)(void *, size_t),
                              int (*errorfunc)(void *)) {
  struct elf_ctx *ctx = kmalloc_track(ELF_LOADER_SITE,sizeof(struct elf_ctx));
  kmemset(ctx, 0, sizeof(struct elf_ctx));
  ctx->readfunc = readfunc;
  ctx->seekfunc = seekfunc;
  ctx->errorfunc = errorfunc;
  list_head_init(&ctx->diversity_units);
  ctx->static_units = qopen();

  return ctx;
}

void free_elf_ctx(struct elf_ctx *ctx) {

  /* Free diversity units. */
  if(!list_empty(&ctx->diversity_units)) {
    struct diversity_unit *i, *dummy;
		
    list_for_each_safe(&ctx->diversity_units, i, dummy, list) {
      list_del(&i->list);
      free_diversity_unit(i);
    }
  }
  qclose(ctx->static_units);

  /* Free the rest. */
  if(ctx->section_headers_loaded) {
    kfree_track(ELF_LOADER_SITE,ctx->section_headers);
    kfree_track(ELF_LOADER_SITE,ctx->unitmap);
  }
  if(ctx->program_headers_loaded)
    kfree_track(ELF_LOADER_SITE,ctx->program_headers);
  if(ctx->section_strtab_loaded)
    kfree_track(ELF_LOADER_SITE,ctx->section_strtab);
  if(ctx->strtab_loaded)
    kfree_track(ELF_LOADER_SITE,ctx->strtab);
  if(ctx->num_syms)
    kfree_track(ELF_LOADER_SITE,ctx->symtab);
  if(ctx->num_rels)
    kfree_track(ELF_LOADER_SITE,ctx->rels);
  if(ctx->num_relas)
    kfree_track(ELF_LOADER_SITE,ctx->relas);
#ifdef REPLICATION
  if(ctx->replication_done){
    kfree_track(ELF_LOADER_SITE,ctx->replicated_shdrs);
  }
#endif
  kfree_track(ELF_LOADER_SITE,ctx);
}

struct diversity_unit *alloc_diversity_unit(struct Elf_Shdr *shdr) {
  struct diversity_unit *unit = kmalloc_track(ELF_LOADER_SITE,sizeof(struct diversity_unit));
  kmemset(unit, 0, sizeof(struct diversity_unit));
  if(shdr != NULL) {
    unit->hdr = shdr;
    unit->addr = shdr->sh_addr;
    unit->memsz = shdr->sh_size;
  }
  list_head_init(&unit->external_relocs);
  return unit;
}

void free_diversity_unit(struct diversity_unit *unit) {
  if(unit->num_rels)
    kfree_track(ELF_LOADER_SITE,unit->rels);
  if(unit->num_relas)
    kfree_track(ELF_LOADER_SITE,unit->relas);
  if(unit->num_syms)
    kfree_track(ELF_LOADER_SITE,unit->syms);
  if(!list_empty(&unit->external_relocs)) {
    struct external_reloc *i, *dummy;
    list_for_each_safe(&unit->external_relocs, i, dummy, list) {
      list_del(&i->list);
      kfree_track(ELF_LOADER_SITE,i);
    }
  }
#ifdef REPLICATION
  if(unit->is_replicated && !(unit->replica))
    kfree_track(ELF_LOADER_SITE,unit->replicas);
#endif
  kfree_track(ELF_LOADER_SITE,unit);
}

/* Check the values in the header, make sure they're correct for what our simple
 * ELF loader can handle. */
static int checkhdr(struct Elf64_Ehdr *hdr) {
  /* Check for the magic number */
  if(hdr->e_ident[EL_MAG0] !='\x7f' ||
     hdr->e_ident[EL_MAG1] != 'E'   ||
     hdr->e_ident[EL_MAG2] != 'L'   ||
     hdr->e_ident[EL_MAG3] != 'F')
    return 1;
  /* Make sure we're class 2 (ELF64) */
  if(hdr->e_ident[EL_CLASS] != ELFCLASS64)
    return 2;
  /* Make sure we're little-endian. */
  if(hdr->e_ident[EL_DATA] != ELFDATA2LSB)
    return 3;
  /* Make sure we're using the SYSV ABI */
  if(hdr->e_ident[EL_OSABI] != ELFOSABI_SYSV)
    return 4;
  /* Make sure we're an executable file and only an executable file */
  if(hdr->e_type != ET_EXEC)
    return 5;

  return 0;
}
/* Invert the above function into a "valid or not" question. */
#define valid_header(x) (checkhdr(x) ? 0 : 1)

static int checkphdr(struct Elf64_Phdr *hdr) {
  /* We don't have a comprehensive enough loader here to deal with segments that aren't
   * PT_LOAD segments (including unwind and whatnot). Rather than just ignoring them,
   * which is probably not a bad way to deal wi the problem, we'll just fail here.
   * When linking the hypervisor, make sure that these sections are discarded.
   * Eventually we'll find a way to deal with them (even if it's just ignoring them). */
  /* One of those control fake headers that we don't care about? */
  if(hdr->p_memsz == 0)
    goto out;

  if(hdr->p_type != PT_LOAD)
    return 1;
#ifdef HYPV
  /* Make sure the hypervisor is to be loaded above 2 MB! */
  if(hdr->p_vaddr < 0x200000)
    return 2;
#endif

 out:
  return 0;
}
/* Invert the above function into a "valid or not" question. */
#define valid_pheader(x) (checkphdr(x) ? 0 : 1)

static int load_file_header(struct elf_ctx *ctx) {
  /* Don't try to load multiple times. */
  if(ctx->file_header_loaded) return 1;

  ctx->seekfunc(ctx->file_ctx, 0);
  ctx->readfunc(ctx->file_ctx, &ctx->file_header, sizeof(struct Elf_Ehdr));
  if(ctx->errorfunc(ctx->file_ctx)) return (1 << 1);

  if(checkhdr(&ctx->file_header)) return (1 << 2);

  ctx->file_header_loaded = 1;
  return 0;
}

static int load_phdrs(struct elf_ctx *ctx) {
  const size_t size = sizeof(struct Elf_Phdr) * ctx->file_header.e_phnum;

  /* Don't try to load multiple times. */
  if(ctx->program_headers_loaded) return 1;

  ctx->program_headers = kmalloc_track(ELF_LOADER_SITE,size);
  kmemset(ctx->program_headers, 0, size);
  ctx->seekfunc(ctx->file_ctx, ctx->file_header.e_phoff);
  if(ctx->errorfunc(ctx->file_ctx)) goto bad;
  ctx->readfunc(ctx->file_ctx, ctx->program_headers, size);
  if(ctx->errorfunc(ctx->file_ctx)) goto bad;

  ctx->program_headers_loaded = 1;
  return 0;
 bad:
  kputs("[System] Error loading program headers.");
  kfree_track(ELF_LOADER_SITE,ctx->program_headers);
  return 1;
}

/* Returns 1 if this section could have a relocation that uses a symbol assigned
 * to it (i.e., usable in a diversity unit). That this function exists is a bit
 * of a hack; it means that we aren't diversifying/relocating literally every
 * loaded section, and this is the way to find out which ones we should be. */
static int relocatable_section(struct elf_ctx *ctx, struct Elf_Shdr *shdr) {
  struct Elf_Phdr *phdr;
  int i;

  /* Make sure it's loadable into memory. */
  if(!(shdr->sh_type == SHT_PROGBITS || shdr->sh_type == SHT_NOBITS))
    return 0;

  /* Make sure it appears in a PT_LOAD segment. */
  for(i = 0, phdr = ctx->program_headers; i < ctx->file_header.e_phnum; i++, phdr++) {
    if(phdr->p_type == PT_LOAD) {
      if(ELF_SECTION_IN_SEGMENT(shdr, phdr)) {
	return 1;
      }
    }
  }
  return 0;
}

static int load_shdrs(struct elf_ctx *ctx) {
  struct Elf_Shdr *shdr;
  int i, seekto;

  /* Don't try to load multiple times. */
  if(ctx->section_headers_loaded) return 1;

  ctx->section_headers = kmalloc_track(ELF_LOADER_SITE,sizeof(struct Elf_Shdr) * ctx->file_header.e_shnum);
  ctx->unitmap = kmalloc_track(ELF_LOADER_SITE,sizeof(struct diversity_unit *) * ctx->file_header.e_shnum);
  kmemset(ctx->unitmap, 0, sizeof(struct diversity_unit *) * ctx->file_header.e_shnum);
  seekto = ctx->file_header.e_shoff;
  for(i = 0; i < ctx->file_header.e_shnum; i++) {
    shdr = ctx->section_headers + i;
    ctx->seekfunc(ctx->file_ctx, seekto);
    if(ctx->errorfunc(ctx->file_ctx)) goto bad;
    ctx->readfunc(ctx->file_ctx,
		  shdr,
		  sizeof(struct Elf_Shdr));
    if(ctx->errorfunc(ctx->file_ctx)) goto bad;
    seekto += sizeof(struct Elf_Shdr);

    /* Diversity stuff: we utilize ELF sections as our "diversity units",
     * which can be relocated. Create such a unit for each section that has
     * a load size, and is either PROGBITS (stuff program uses itself) or
     * NOBITS (covers at least .bss). This should in theory cover everything
     * that could have a relocation. */
    if(relocatable_section(ctx, shdr) && shdr->sh_size) {
      struct diversity_unit *unit = alloc_diversity_unit(shdr);
      list_add_tail(&ctx->diversity_units, &unit->list);
      ctx->num_diversity_units++;
      *(ctx->unitmap + i) = unit;
    }

    continue;

  bad:
    kprintf("[System] Error loading section header %d.\n", i);
    kfree_track(ELF_LOADER_SITE,ctx->section_headers);
    return 1;
  }

  ctx->section_headers_loaded = 1;
  return 0;
}

/* Load the string table for the section names, and fix up the section data we've read so it points to
 * the correct names. */
static int load_section_strtab(struct elf_ctx *ctx) {
  struct Elf_Shdr *shdr;
  int seekto;

  if(!(ctx->section_headers_loaded)) {
    kputs("[System] load_section_strtab: precondition not met.");
    return 1;
  }
  if(ctx->section_strtab_loaded) {
    kputs("[System] load_section_strtab: attempted to load twice.");
    return 2;
  }
  if(ctx->file_header.e_shstrndx == SHN_UNDEF) {
    kputs("[System] Undefined shstrtab.");
    return 3;
  }
  if(ctx->file_header.e_shstrndx > ctx->file_header.e_shnum) {
    kputs("[System]: shstrndx greater than highest section header.");
    return 4;
  }

  /* Load section string table; its location is in the file header. */
  shdr = ctx->section_headers + ctx->file_header.e_shstrndx;
  seekto = shdr->sh_offset;
  ctx->section_strtab = kmalloc_track(ELF_LOADER_SITE,shdr->sh_size);
  ctx->seekfunc(ctx->file_ctx, seekto);
  ctx->readfunc(ctx->file_ctx, ctx->section_strtab, shdr->sh_size);
  if(ctx->errorfunc(ctx->file_ctx)) {
    kfree_track(ELF_LOADER_SITE,ctx->section_strtab);
    kputs("[System] Error while reading shstrtab.");
    return 5;
  }
  ctx->section_strtab_loaded = 1;

  return 0;
}

/* Load the symbol table and its associated strings. */
static int load_symtab(struct elf_ctx *ctx) {
  struct Elf_Shdr *shdr;
  int i;

  if(ctx->strtab_loaded || ctx->num_syms) {
    kprintf("[System] Attempted to load string or symbol table twice.\n");
    return 1;
  }

  /* Load the string table first, so we can immediately point the symbols to their names. */
  for(i = 0, shdr = ctx->section_headers; i < ctx->file_header.e_shnum; i++, shdr++) {
    //kprintf("sh_name = %s\n", ELF_SECNAME(*shdr, ctx->section_strtab));
    if(shdr->sh_type == SHT_STRTAB &&
       kstreq(ELF_SECNAME(*shdr, ctx->section_strtab), ".strtab")) {
      ctx->strtab = kmalloc_track(ELF_LOADER_SITE,shdr->sh_size);
      ctx->seekfunc(ctx->file_ctx, shdr->sh_offset);
      ctx->readfunc(ctx->file_ctx, ctx->strtab, shdr->sh_size);
      ctx->strtab_loaded = 1;
      if(ctx->errorfunc(ctx->file_ctx)) {
	kprintf("[System] Error loading string table %s\n",
		ELF_SECNAME(*shdr, ctx->section_strtab));
	kfree_track(ELF_LOADER_SITE,ctx->strtab);
	ctx->strtab_loaded = 0;
      }
      break;
    }
  }
  if(!ctx->strtab_loaded) {
    kprintf("[System] Invalid or nonexistant string table.\n");
    return 2;
  }

  /* Now load the symbol table. */
  for(i = 0, shdr = ctx->section_headers; i < ctx->file_header.e_shnum; i++, shdr++) {
    //kprintf("symbol name %s\n", 
    //	       ELF_SECNAME(*shdr, ctx->section_strtab));
    if(shdr->sh_type == SHT_SYMTAB &&
       kstreq(ELF_SECNAME(*shdr, ctx->section_strtab), ".symtab")) {
      ctx->symtab = kmalloc_track(ELF_LOADER_SITE,shdr->sh_size);
      ctx->seekfunc(ctx->file_ctx, shdr->sh_offset);
      ctx->readfunc(ctx->file_ctx, ctx->symtab, shdr->sh_size);
      ctx->num_syms = shdr->sh_size / sizeof(struct Elf_Sym);
      if(ctx->errorfunc(ctx->file_ctx)) {
	kprintf("[System] Error loading symbol table %s\n",
		ELF_SECNAME(*shdr, ctx->section_strtab));
	kfree_track(ELF_LOADER_SITE,ctx->symtab);
	ctx->num_syms = 0;
      }
      break;
    }
  }
  if(!ctx->num_syms) {
    kprintf("[System] Invalid or nonexistant symbol table.\n");
    return 3;
  }

  return 0;
}

/* Load a .rel(a) section and tie it to its associated diversity unit. */
static int load_rel_section(struct elf_ctx *ctx, struct Elf_Shdr *shdr) {
  struct diversity_unit *unit;
  int num_rels;

  /* sh_info is the offset into the sections list for the section that "owns"
   * these relocations. It's also the offset into our own sections -> units
   * map to return the "owning" unit (which itself has a pointer to the owning
   * section). */
  unit = *(ctx->unitmap + shdr->sh_info);
  if(unit == NULL) {
    /* This is a warning, not an error. This is because there can be
     * relocation sections for sections that didn't actually appear in
     * loaded segments (like debug info). */
#ifdef ELF_DEBUG
    kprintf("[System] No text section corresponding to relocation section %s "
	    "(pointed-to section: %s)\n",
	    ELF_SECNAME(*shdr, ctx->section_strtab),
	    ELF_SECNAME(*(ctx->section_headers + shdr->sh_info), ctx->section_strtab));
#endif
    return 0;
  }

  num_rels = shdr->sh_size / sizeof(struct Elf_Rel);
  ctx->num_rels += num_rels;
  unit->rel_hdr = shdr;
  unit->num_rels = num_rels;
  unit->rels = kmalloc_track(ELF_LOADER_SITE,shdr->sh_size);

  /* Read in the relocations. */
  ctx->seekfunc(ctx->file_ctx, shdr->sh_offset);
  ctx->readfunc(ctx->file_ctx, unit->rels, shdr->sh_size);
  if(ctx->errorfunc(ctx->file_ctx)) {
    kprintf("[System] Error loading relocations %s.\n",
	    ELF_SECNAME(*shdr, ctx->section_strtab));
    return 1;
  }
  return 0;
}

static int load_rela_section(struct elf_ctx *ctx, struct Elf_Shdr *shdr) {
  struct diversity_unit *unit;
  int num_relas;

  /* sh_info is the offset into the sections list for the section that "owns"
   * these relocations. It's also the offset into our own sections -> units
   * map to return the "owning" unit (which itself has a pointer to the owning
   * section). */
  unit = *(ctx->unitmap + shdr->sh_info);
  if(unit == NULL) {
    /* This is a warning, not an error. This is because there can be
     * relocation sections for sections that didn't actually appear in
     * loaded segments (like debug info). */
#ifdef ELF_DEBUG
    kprintf("[System] No text section corresponding to relocation section %s "
	    "(pointed-to section: %s)\n",
	    ELF_SECNAME(*shdr, ctx->section_strtab),
	    ELF_SECNAME(*(ctx->section_headers + shdr->sh_info), ctx->section_strtab));
#endif
    return 0;
  }

  num_relas = shdr->sh_size / sizeof(struct Elf_Rela);
  ctx->num_relas += num_relas;
  unit->rela_hdr = shdr;
  unit->num_relas = num_relas;
  unit->relas = kmalloc_track(ELF_LOADER_SITE,shdr->sh_size);

  /* Read in the relocations. */
  ctx->seekfunc(ctx->file_ctx, shdr->sh_offset);
  ctx->readfunc(ctx->file_ctx, unit->relas, shdr->sh_size);
  if(ctx->errorfunc(ctx->file_ctx)) {
    kprintf("[System] Error loading relocations %s.\n",
	    ELF_SECNAME(*shdr, ctx->section_strtab));
    return 1;
  }

  return 0;
}

/* Load all the relocations. After relocation loading is done, set up the
 * associated symbol and relocation lists that we need to use for generating
 * diversity.
 */
static int load_relocs(struct elf_ctx *ctx) {
  struct Elf_Shdr *shdr;
  struct diversity_unit *unit;
  size_t loaded_rels = 0;
  size_t loaded_relas = 0;
  int i;

  if(ctx->relocations_processed) return 1;

  /* Loop through and read every .rel(a) section. */
  for(i = 0, shdr = ctx->section_headers; i < ctx->file_header.e_shnum; i++, shdr++) {
    int error;
    switch(shdr->sh_type) {
    case SHT_REL:
      error = load_rel_section(ctx, shdr);
      if(error) goto bad;
      break;
    case SHT_RELA:
      error = load_rela_section(ctx, shdr);
      if(error) goto bad;
      break;
    }
  }

  /* Now concatenate every relocation that we have read into one big list, and
   * create the maps from relocations -> diversity units. Inserting into the
   * maps is deferred until later, because diversity changes them. */
  ctx->rels = kmalloc_track(ELF_LOADER_SITE,ctx->num_rels * sizeof(struct Elf_Rel *));
  ctx->relas = kmalloc_track(ELF_LOADER_SITE,ctx->num_relas * sizeof(struct Elf_Rela *));
  list_for_each(&ctx->diversity_units, unit, list) {
    for(i = 0; i < unit->num_rels; i++)
      *(ctx->rels + loaded_rels + i) = unit->rels + i;
    for(i = 0; i < unit->num_relas; i++)
      *(ctx->relas + loaded_relas + i) = unit->relas + i;

    loaded_rels += unit->num_rels;
    loaded_relas += unit->num_relas;
  }

  ctx->relocations_processed = 1;
  return 0;

 bad:
  kputs("[System] Error loading relocations.");
  /* Free any relocations caught in the diversity units. */
  list_for_each(&ctx->diversity_units, unit, list) {
    if(unit->num_rels)
      kfree_track(ELF_LOADER_SITE,unit->rels);
    if(unit->num_relas)
      kfree_track(ELF_LOADER_SITE,unit->relas);
  }
  ctx->num_rels = 0;
  ctx->num_relas = 0;
  return 1;
}

static void process_diversity_units(struct elf_ctx *ctx) {
  struct Elf_Shdr *shdr;
  struct Elf_Rel *rel;
  struct Elf_Rela *rela;
  struct Elf_Sym *sym;
  struct diversity_unit *unit;
  struct external_reloc *listitem;
  int *symnum;
  int i;

  /* Check preconditions. */
  if(ctx->diversity_units_processed) return;
  if(!(ctx->num_syms)) return;
  if(!ctx->num_rels && !ctx->num_relas) return;

  /* Now that the symbol table is loaded, we want to assign symbols to their
   * correct diversity units. For each symbol, take its section and map it to
   * its unit. We need to run this in two passes: first to calculate how many
   * symbols go to the unit (so we can allocate a correctly-sized array), and
   * second to place the symbol pointers in the array. */
  for(i = 0, sym = ctx->symtab; i < ctx->num_syms; i++, sym++) {
    /* Make sure it's a valid diversity symbol (must be contained in a valid
     * section / have a valid section header). */
    if(sym->st_shndx == SHN_UNDEF)
      continue;
    if(sym->st_shndx >= ctx->file_header.e_shnum)
      continue;
    unit = *(ctx->unitmap + sym->st_shndx);
    if(unit == NULL)
      continue;
    unit->num_syms++;
  }

  /* Allocate the symbol pointer lists. */
  list_for_each(&ctx->diversity_units, unit, list) {
    unit->syms = kmalloc_track(ELF_LOADER_SITE,unit->num_syms * sizeof(struct Elf_Sym *));
    kmemset(unit->syms, 0, unit->num_syms * sizeof(struct Elf_Sym *));
  }

  /* Populate the symbol pointer lists. Now that the symbol table is loaded,
   * we want to assign symbols to their correct diversity units. Go through
   * the symbols and grab the associated diversity unit (this is the second
   * such loop), and add it at the tail of the symbol list in that unit. Said
   * tail is tracked by the symnum array, whose index corresponds to the index
   * in unitmap. */
  symnum = kmalloc_track(ELF_LOADER_SITE,sizeof(int) * ctx->file_header.e_shnum);
  kmemset(symnum, 0, sizeof(int) * ctx->file_header.e_shnum);
  for(i = 0, sym = ctx->symtab; i < ctx->num_syms; i++, sym++) {
    /* Make sure it's a valid diversity symbol (must be contained in a valid
     * section / have a valid section header). */
    if(sym->st_shndx == SHN_UNDEF)
      continue;
    if(sym->st_shndx >= ctx->file_header.e_shnum)
      continue;
    unit = *(ctx->unitmap + sym->st_shndx);
    if(unit == NULL)
      continue;
    *(unit->syms + *(symnum + sym->st_shndx)) = sym;
    *(symnum + sym->st_shndx) += 1;
  }
  kfree_track(ELF_LOADER_SITE,symnum);

  /* Finally, set up the external symbol lists in the diversity units. Each
   * unit has a list of relocations that must be processed and the pointed
   * code updated when the UNIT ITSELF is moved.
   * NOTE: This spot could be a point of error messages in the future. If the
   * loader complains about an error processing a relocation, look here -- it
   * means there's a missing diversity unit, so we aren't diversifying
   * something that we should. */
  for(i = 0; i < ctx->num_rels; i++) {
    rel = *(ctx->rels + i);
    sym = ctx->symtab + ELF64_R_SYM(rel->r_info);
    shdr = ctx->section_headers + sym->st_shndx;

    /* Skip absolute (ABS) symbols; they are not supposed to be relocated.
     * Specific absolute symbols that we might want to diversify (such as
     * "end", which marks the heap start) need to get picked up later on
     * in the process manager once we create diversity units for them,
     * similar to how the stack gets diversified. */
    if(sym->st_shndx == SHN_ABS) continue;

    /* Find diversity unit for symbol. */
    unit = *(ctx->unitmap + sym->st_shndx);
    if(unit == NULL) {
      /* This should never happen; all relocations should point to a valid
       * diversity unit. In theory, anyway. */
#ifdef ELF_DEBUG
      kprintf("[System] Error processing relocation with offset %x; symbol was %s; shdr %s.\n",
	      rel->r_offset, ELF_SYMNAME(*sym, ctx->strtab), ELF_SECNAME(*shdr, ctx->section_strtab));
#endif
      continue;
    }

    listitem = kmalloc_track(ELF_LOADER_SITE,sizeof(struct external_reloc));
    kmemset(listitem, 0, sizeof(struct external_reloc));
    listitem->rel = rel;
    listitem->type = REL;
    list_add_tail(&unit->external_relocs, &listitem->list);
  }
  for(i = 0; i < ctx->num_relas; i++) {
    rela = *(ctx->relas + i);
    sym = ctx->symtab + ELF64_R_SYM(rela->r_info);
    shdr = ctx->section_headers + sym->st_shndx;

    /* Skip absolute (ABS) symbols; they are not supposed to be relocated.
     * Specific absolute symbols that we might want to diversify (such as
     * "end", which marks the heap start) need to get picked up later on
     * in the process manager once we create diversity units for them,
     * similar to how the stack gets diversified. */
    if(sym->st_shndx == SHN_ABS)
      continue;

    /* Find diversity unit for symbol. */
    unit = *(ctx->unitmap + sym->st_shndx);
    if(unit == NULL) {
      /* This should never happen; all relocations should point to a valid
       * diversity unit. HOWEVER, this can at times happen in user procs.
       * Basically, you're allowed to relocate against a weak symbol and,
       * if this symbol is undefined, it's not an error -- instead, you
       * act as if it had value zero. These have the value zero by
       * default in the executable code, so there's no real issue here. */
#ifdef ELF_DEBUG /*kernel user procs have this message*/
      kprintf("[System] Error processing relocation with offset %x; symbol was %s; shdr %s.\n",
	      rela->r_offset, ELF_SYMNAME(*sym, ctx->strtab), ELF_SECNAME(*shdr, ctx->section_strtab));
#endif
      continue;
    }

    listitem = kmalloc_track(ELF_LOADER_SITE,sizeof(struct external_reloc));
    kmemset(listitem, 0, sizeof(struct external_reloc));
    listitem->rela = rela;
    listitem->type = RELA;
    list_add_tail(&unit->external_relocs, &listitem->list);
  }

  ctx->diversity_units_processed = 1;
}

/* External function designed to allow a caller to load all the data from an ELF
 * file without actually loading the sections or executing it.
 */
int elf_load_metadata(struct elf_ctx *ctx) {
  int status = 0;

  if(!(ctx->file_header_loaded)) 
    status |= load_file_header(ctx);

  if(!(ctx->program_headers_loaded)) 
    status |= load_phdrs(ctx);

  if(!(ctx->section_headers_loaded))
    status |= load_shdrs(ctx);

  if(!(ctx->section_strtab_loaded))
    status |= load_section_strtab(ctx);

  if(!(ctx->num_syms))
    status |= load_symtab(ctx);

  if(!(ctx->relocations_processed))
    status |= load_relocs(ctx);

  /* Process the diversity units at this point, populating them with any
   * remaining data that they require. */
  if(!(ctx->diversity_units_processed))
    process_diversity_units(ctx);

  return status;
}

static int load_segment(
#ifdef KERNEL
			void *proc, 
#endif
			struct elf_ctx *ctx, struct Elf_Phdr *hdr) {
  int size, i;
  int read, write, exec =0;
  struct Elf_Shdr *shdr;

  /* Get to the necessary point in the file to load this segment. */
  ctx->seekfunc(ctx->file_ctx, hdr->p_offset);
  if(ctx->errorfunc(ctx->file_ctx))
    return ctx->errorfunc(ctx->file_ctx);

  /* Permission bits from the elf header. */
  if(hdr->p_flags & PF_R)
    read = 1;
  if(hdr->p_flags & PF_W)
    write = 1;
  if(hdr->p_flags & PF_X)
    exec = 1;

  /* How many pages we need to set. */
  size = hdr->p_filesz;
  if(size % PAGE_SIZE)
    size += PAGE_SIZE - (size % PAGE_SIZE);

  /* not much point in reading in zero bytes */
  if(hdr->p_filesz == 0)
    return 0;
	
  /* allocate the virtual memory space */
  /* TODO: Global data is executable */
  /* TODO: code is writeable? -__- */
#ifdef BOOTLOADER
  vmem_alloc((uint64_t*)hdr->p_vaddr, size, PG_RW | PG_GLOBAL);
#else
  vmem_alloc((uint64_t*)hdr->p_vaddr, size, PG_RW | PG_USER);
#endif

#ifdef KERNEL
  add_memory_region(proc, TEXT_REGION, USR_TEXT_PERMS, hdr->p_vaddr, hdr->p_vaddr + size);
#endif

  ctx->readfunc(ctx->file_ctx, (void*)hdr->p_vaddr, size);
  if(ctx->errorfunc(ctx->file_ctx)) {
    return ctx->errorfunc(ctx->file_ctx);
  }	
  kmemset((void*)(hdr->p_vaddr + hdr->p_filesz), 0, size - hdr->p_filesz);

  /* map in the BSS */
  for(i = 0, shdr = ctx->section_headers; i < ctx->file_header.e_shnum; i++, shdr++) {
    if(kstreq(ELF_SECNAME(*shdr, ctx->section_strtab), ".bss")) {
#ifdef KERNEL
      vmem_alloc( (uint64_t*)shdr->sh_addr, shdr->sh_size, 
		  PG_RW | PG_USER | PG_NX);
      add_memory_region(proc, BSS_REGION, PG_USER | PG_RW | PG_NX, 
			shdr->sh_addr, shdr->sh_addr + shdr->sh_size);
#else
      vmem_alloc( (uint64_t*)shdr->sh_addr, shdr->sh_size, 
		  PG_RW | PG_GLOBAL | PG_NX );
#endif
      kmemset( (void*)shdr->sh_addr, 0, shdr->sh_size );
      break;
    }
  }

  return 0;
}

#ifdef HYPV
static int check_memory(struct bad_region *brs, size_t num_brs,
                        uint64_t mem_start, uint64_t mem_len) {
  int i;

  for(i = 0; i < num_brs; i++, brs++) {
    /* Case 1: Bad region is entirely after load region. */
    if(brs->start > mem_start + mem_len)
      continue;
    /* Case 2: Bad region is entirely before load region. */
    else if(brs->end < mem_start)
      continue;
    /* Case 3: Otherwise, it intersects. */
    else
      return 1;
  }
  return 0;
}
#endif

/**TODO bad regions what are they and do we need them? may be vestigial limb */
uint64_t elf_load_file(
#ifndef BOOTLOADER 
		       void *proc, 
#endif
		       struct elf_ctx *ctx
#ifdef HYPV 
		       , struct bad_region *brs, size_t num_brs
#endif
		       ) {

  struct Elf_Phdr *phdr;
  int i, rc;


  /* Load and store the headers, if not already done. */
  rc = elf_load_metadata(ctx);
  if(rc) {
    kprintf("Failed to load metadata from ELF file\n");
    return MEM_FAIL;
  }

  /* Check that each program header that we have read does not cross into a bad memory
   * region. If it does not, read that segment into memory. Make sure that important
   * things like OUR kernel code and OUR kernel stack are marked as bad memory
   * regions! */
  for(i = 0; i < ctx->file_header.e_phnum; i++) {
    phdr = ctx->program_headers + i;

    rc = valid_pheader(phdr);
    if(!rc) {
      continue;
    }

#ifdef HYPV
    rc = check_memory(brs, num_brs,
		      phdr->p_vaddr,
		      phdr->p_memsz);
    if(rc) {
      kprintf("Error loading header number %d: memory conflict.\n", i);
      return 3;
    }
#endif

#ifdef KERNEL
    rc = load_segment( (void*)proc, ctx, phdr);
#else
    rc = load_segment( ctx, phdr );
#endif

    if(rc) {
      kprintf("error code %d", rc);
      return MEM_FAIL;
    }
  }

  return ctx->file_header.e_entry;
}

/* Utility function for determining the size of a relocation. */
int reloc_size(int reloctype) {
  switch(reloctype) {
  case R_X86_64_TLSDESC:
    return 128;
  case R_X86_64_64:
  case R_X86_64_DTPMOD64:
  case R_X86_64_DTPOFF64:
  case R_X86_64_GLOB_DAT:
  case R_X86_64_GOTOFF64:
  case R_X86_64_JUMP_SLOT:
  case R_X86_64_PC64:
  case R_X86_64_RELATIVE:
  case R_X86_64_TPOFF64:
    return 64;
  case R_X86_64_32:
  case R_X86_64_32S:
  case R_X86_64_DTPOFF32:
  case R_X86_64_GOT32:
  case R_X86_64_GOTPC32:
  case R_X86_64_GOTPC32_TLSDESC:
  case R_X86_64_GOTPCREL:
  case R_X86_64_GOTTPOFF:
  case R_X86_64_PC32:
  case R_X86_64_PLT32:
  case R_X86_64_SIZE32:
  case R_X86_64_TLSGD:
  case R_X86_64_TLSLD:
  case R_X86_64_TPOFF32:
    return 32;
  case R_X86_64_16:
  case R_X86_64_PC16:
    return 16;
  case R_X86_64_8:
  case R_X86_64_PC8:
    return 8;
  case R_X86_64_COPY:
  case R_X86_64_NONE:
  case R_X86_64_TLSDESC_CALL:
    return 0;
  default:
    kprintf("[System] Attempted to take size of unknown relocation (type: %d).\n", reloctype);
  }

  return 0;
}

#define checkstatus(x) if(file_error_check(x)) return MEM_FAIL
uint64_t get_symbol_addr(void* file_ctx, char* sym_name, char* string_table){

  struct Elf64_Ehdr file_header;
  struct Elf64_Shdr section_header;
  struct Elf64_Sym  symbol;		
  int i, seekto, len, rc;

  /*start back at zero point in file in case it was jacked with!!!! */
  file_seek(file_ctx, 0);
  /*read in the main file header */
  file_read(file_ctx, &file_header, sizeof(struct Elf64_Ehdr));
  //checkstatus(file_ctx);
  rc = checkhdr(&file_header);
  if(rc) return MEM_FAIL;

  /*set the seek to the index of the sections */
  seekto = file_header.e_shoff;

  /*length of the symbol we're given so we can run safe kstrncmp */
  len =  kstrlen(sym_name);
  if(len == 0){
    /*sanity check in case user is retarted */
    return MEM_FAIL;
  }
  /*loop over the various sections */
  for(i=0; i < file_header.e_shnum;i++){
    file_seek(file_ctx, seekto);
    file_read(file_ctx, &section_header, sizeof(struct Elf64_Shdr));
    /*until we find the symbol table sectonn */
    if(section_header.sh_type ==0x2){
      seekto = section_header.sh_offset; 
      /*now loop over the symbols (in HEX dumbass)*/
      for( i=0; i < section_header.sh_size / section_header.sh_entsize; i++){
	file_seek(file_ctx, seekto);
	file_read(file_ctx, &symbol, sizeof(struct Elf64_Sym));
				
	if(symbol.st_value != 0 ){
	  /*see if our name is in the list */
	  if(kstrncmp(string_table+symbol.st_name, sym_name, len) == 0){
	    /*							
	     * kprintf("name check 2%s ", &string_table[symbol.st_name]);
	     * kprintf("addr %x \n", symbol.st_value);
	     */	
	    return symbol.st_value;
	  }
	}
	/*otherwise jump to the next symbol */	
	seekto += sizeof(struct Elf64_Sym);
      } /*SYMBOL loop */ 
      /*if we got here the symbol wasnt found */
      return MEM_FAIL;  /* symbol not found */
    }
    else
      seekto += sizeof(struct Elf64_Shdr);
  }/*SECTION loop */
  return MEM_FAIL;
}

static void swap (uint64_t a[], int left, int right)
{
  uint64_t temp;
  temp=a[left];
  a[left]=a[right];
  a[right]=temp;
}//end swap

static int partition( uint64_t a[], int low, int high )
{
  uint64_t left, right;
  uint64_t pivot_item;

  pivot_item = a[low];
  right = high;
  while ( left < right )
    {
      // Move left while item < pivot
      while( a[left] <= pivot_item )
	left++;
      // Move right while item > pivot
      while( a[right] > pivot_item )
	right--;
      if ( left < right )
	swap(a,left,right);
    }
  // right is final position for the pivot
  a[low] = a[right];
  a[right] = pivot_item;
  return right;
}//end partition 

static void quicksort( uint64_t a[], int low, int high )
{
  uint64_t pivot;
  // Termination condition!
  if ( high > low )
    {
      pivot = partition( a, low, high );
      quicksort( a, low, pivot-1 );
      quicksort( a, pivot+1, high );
    }
} //end quicksort

uint64_t* symbol_addr_list(void *file_ctx, int *sym_count){

  struct Elf64_Ehdr file_header;
  struct Elf64_Shdr section_header;
  struct Elf64_Sym  symbol;		
  int i, seekto, rc;
  uint64_t *sym_array; 
  
  /*start back at zero point in file in case it was jacked with!!!! */
  file_seek(file_ctx, 0);
  /*read in the main file header */
  file_read(file_ctx, &file_header, sizeof(struct Elf64_Ehdr));
  //checkstatus(file_ctx);
  rc = checkhdr(&file_header);
  if(rc) return NULL;
  
  /*set the seek to the index of the sections */
  seekto = file_header.e_shoff;
  
  /*loop over the various sections */
  for(i=0; i < file_header.e_shnum;i++){
    file_seek(file_ctx, seekto);
    file_read(file_ctx, &section_header, sizeof(struct Elf64_Shdr));
    /*until we find the symbol table sectonn */
    if(section_header.sh_type ==0x2){
      seekto = section_header.sh_offset;
      /*determine the size of the array and make space for it*/
      sym_array = (uint64_t*)kmalloc_track(ELF_LOADER_SITE,
					   ((section_header.sh_size/section_header.sh_entsize)*sizeof(uint64_t)));
      /*now loop over the symbols (in HEX dumbass)*/
      for( i=0; i < section_header.sh_size / section_header.sh_entsize; i++){
	file_seek(file_ctx, seekto);
	file_read(file_ctx, &symbol, sizeof(struct Elf64_Sym));
	
	sym_array[i] = symbol.st_value;
	/*otherwise jump to the next symbol */	
	seekto += sizeof(struct Elf64_Sym);
      } /*SYMBOL loop */ 
      *sym_count = i;
      quicksort(sym_array, 0, i-1); 	
      return sym_array;  
    }
    else
      seekto += sizeof(struct Elf64_Shdr);
  }/*SECTION loop */
  return NULL;
}

char * search_symbol_name(uint64_t address, char *file_name){

  int i, sym_count;
  uint64_t temp;
  char *string_table, *name;
  uint64_t *sym_array;
  FIL file_ctx;
  int rc;

  rc = f_open(&file_ctx, file_name, FA_READ);
	
  if(rc){
    kprintf("read file failed in search symbol with err code %d\n", rc);
    return NULL;
  }	
  sym_array = symbol_addr_list(&file_ctx, &sym_count);
  string_table = read_string_table(&file_ctx);
		
  temp =0;
  for(i =0; i<sym_count-2; i++){
    //	  if(address >= sym_array[sym_count]) { 
    //temp = sym_array[sym_count];
    //break;
    //	  }
    if((address >= sym_array[i]) && (address < sym_array[i+1]) ) {
      temp = sym_array[i]; 
      break;
    }
  }
	
  name = get_symbol_name(&file_ctx, temp, string_table);

  f_close(&file_ctx);
  kfree_track(ELF_LOADER_SITE,string_table);
  kfree_track(ELF_LOADER_SITE,sym_array);

  return name;
}

char *get_symbol_name(void *file_ctx,  uint64_t address, char * string_table){
  struct Elf64_Ehdr file_header;
  struct Elf64_Shdr section_header;
  struct Elf64_Sym  symbol;
  int seekto, i, rc; 
	
  /*read in the main file header */
  file_seek(file_ctx, 0);
  file_read(file_ctx, &file_header, sizeof(struct Elf64_Ehdr));
  //checkstatus(file_ctx);
  rc = checkhdr(&file_header);
  if(rc){
    kprintf("read file failed in get symbol with err code %d\n", rc);
    return NULL;
  }	
  /*set the seek to the index of the sections */
  seekto = file_header.e_shoff;

  /*loop over the various sections */
  for(i=0; i < file_header.e_shnum;i++){
    file_seek(file_ctx, seekto);
    file_read(file_ctx, &section_header, sizeof(struct Elf64_Shdr));
    /*until we find the symbol table sectonn */
    if(section_header.sh_type ==0x2){
      seekto = section_header.sh_offset; 
      /*now loop over the symbols */
      for( i=0x0; i < section_header.sh_size / section_header.sh_entsize; i++){
	file_seek(file_ctx, seekto);
	file_read(file_ctx, &symbol, sizeof(struct Elf64_Sym));
				
	if(symbol.st_value == address && (symbol.st_info & 0xF) == 2 ){
	  // kprintf("Address is  %x offset %x, Name %s \n", symbol.st_value, symbol.st_name,
	  // (char*)(string_table + symbol.st_name)); 
	  return string_table + symbol.st_name;
	}
	/*otherwise jump to the next symbol */
	seekto += sizeof(struct Elf64_Sym);
      } /* close out symbol loop */
    }/*close out the if type */
    else
      seekto += sizeof(struct Elf64_Shdr);
  }
  return NULL;
}

char *read_string_table(void *file_ctx) {
  struct Elf64_Ehdr file_header;
  struct Elf64_Shdr section_header;
  int seekto, rc;
  char *string_list;
  int i ; 
  file_seek(file_ctx, 0);
  /*read in the main file header */
  file_read(file_ctx, &file_header, sizeof(struct Elf64_Ehdr));
  //checkstatus(file_ctx);
  rc = checkhdr(&file_header);
  /*(SK) will look at later*/
  if(rc) return NULL;//MEM_FAIL;

  /*set the seek to the index of the sections */
  seekto = file_header.e_shoff;

  for(i=0; i < file_header.e_shnum;i++){
    file_seek(file_ctx, seekto);
    file_read(file_ctx, &section_header, sizeof(struct Elf64_Shdr));
    if(i == file_header.e_shnum - 1){
      seekto = section_header.sh_offset; 		
      file_seek(file_ctx, seekto);
      string_list = ( char *)kmalloc_track(ELF_LOADER_SITE,section_header.sh_size);
      file_read(file_ctx, string_list, section_header.sh_size );
      file_seek(file_ctx, 0);
      return string_list;
    }
    else
      seekto += sizeof(struct Elf64_Shdr);
  }
  /*somehow it fell on it's face HARD */
  file_seek(file_ctx, 0);
  /*(SK) will look at later*/
  return NULL;//MEM_FAIL;
}
#undef checkstatus

char *get_section_name(uint64_t offset, char * string_table) {
  return string_table + offset;
}

char *read_section_table(void *file_ctx) {
  struct Elf64_Shdr section_header;
  struct Elf64_Ehdr file_header;
  int seekto, i;
  char *string_list;
  /*just in case it wasn't zero before we start */	
  file_seek(file_ctx, 0);
  /*read in the main file header */
  file_read(file_ctx, &file_header, sizeof(struct Elf64_Ehdr));

  /*set the seek to the index of the sections */
  seekto = file_header.e_shoff;

  for(i=0; i < file_header.e_shnum;i++){
    file_seek(file_ctx, seekto);
    file_read(file_ctx, &section_header, sizeof(struct Elf64_Shdr));
    if(i == file_header.e_shstrndx){
      seekto = section_header.sh_offset; 		
      file_seek(file_ctx, seekto);
      string_list = ( char *)kmalloc_track(ELF_LOADER_SITE,section_header.sh_size);
      file_read(file_ctx, string_list, section_header.sh_size );
      file_seek(file_ctx, 0);
      return string_list;
    }
    else
      seekto += sizeof(struct Elf64_Shdr);

  }
  /*(SK) will look at later*/
  return NULL;//MEM_FAIL;
}

/**************  examples of how to use lookup functions   ********

	string_table = read_string_table(file_ctx);	

 	addr = 0x8850;
       name =   get_symbol_name(file_ctx, addr, string_table);
	if(name != MEM_FAIL)
	kprintf("name is %s", name);


	addr = get_symbol_addr(file_ctx, "freelistAdd", string_table);
	if(addr != MEM_FAIL)
	kprintf("addr is %x ", addr);

	addr =  get_symbol_addr(file_ctx, "vga_get_crtc", string_table);
	if(addr != MEM_FAIL)
	kprintf("addr is %x ", addr);

	kfree_track(ELF_LOADER_SITE,string_table);
*****************************************************************/



