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

#include <diversity.h>
#include <elf_loader.h>
#include <khash.h>
#include <asm_subroutines.h>
#include <kmalloc.h>
#include <kstdio.h>
#include <kstring.h>
#include <list.h>
#include <kqueue.h>
#include <random.h>
#include <file_abstraction.h>
#include <procman.h>

// #define DIVERSITY_DEBUG 1

static char* kitoa(uint64_t val);

#if defined(KPLT) && defined(BOOTLOADER)

volatile uint64_t kplt;

/* the stupid intra-relocation issue forces me to limit my condition to fns 
   without suck a relocation. There are many more, but I didn't want to 
   manually search the 800+ sections for which one would work this is a good 
   sample of functions that are used frequently. Think kernel_exit after every 
   interrrupt including the systick... */
#define CONDITION ( \
  !kstreq(ELF_SECNAME(*unit->hdr, ((Proc_t*)proc)->file->section_strtab), ".text") && \
!kstreq(ELF_SECNAME(*unit->hdr, ((Proc_t*)proc)->file->section_strtab), ".text.kprintf")   && \
!kstreq(ELF_SECNAME(*unit->hdr, ((Proc_t*)proc)->file->section_strtab), ".text.do_print") )

#endif

#define DEBUG_COND ( kstreq(ELF_SECNAME(*unit->hdr, ((Proc_t*)proc)->file->section_strtab), ".text.fputs") )

/******************************************************************************
 *
 * Function: move_unit
 *
 * Description: Given a diversity unit and a current position in memory, move
 *              the unit to that position. Assumes that memory below that unit
 *              is free to use.
 *
 *****************************************************************************/
static void move_unit(Proc_t *proc, struct diversity_unit *unit,
		      uint64_t position) {
  int i;
#if defined(KPLT) && defined(BOOTLOADER)
  uint64_t orig = 0x0;
  int j;
  unsigned char jmp[20];
#endif

  /* Update our unit's symbols. */
  for(i = 0; i < unit->num_syms; i++) {
    struct Elf64_Sym *sym = *(unit->syms + i);
    uint64_t sym_offset = sym->st_value - unit->addr;

#if defined(KPLT) && defined(BOOTLOADER)
    orig = 0;

    /* If this section belongs in the KPLT */
    if ( CONDITION  && 
	 (ELF64_ST_TYPE(sym->st_info) == STT_FUNC) && 
	 (ELF64_ST_BIND(sym->st_info) == STB_GLOBAL)) {

      /* build this table entry */
      jmp[0] = 0x48;
      jmp[1] = 0xb8;
      for ( j = 2; j < 10; j++ )
	jmp[j] = ((position + sym_offset) >> ((j-2)*8)) & 0xFF;
      jmp[10] = 0xff;
      jmp[11] = 0xe0;
      *(uint32_t*)(&jmp[12]) = sym->st_size & ~(uint32_t)0x0;
      *(uint32_t*)(&jmp[16]) = unit->hdr->sh_addralign;

      /* write this table entry */
      kmemcpy((void*)kplt, (void*)jmp, 20);

      /* set the value of the symbol to point to the table */
      orig = sym->st_value;
      sym->st_value = kplt;

#if 0
      /* Unfortunately, this is not working right. Every static function should get its own entry in the 
       * PLT. We need some more logic to dinstinguish this. 
       */
      for ( j = 0; j < proc->file->num_syms; j++ ) 
	if ( orig && (((proc->file->symtab + j)->st_value == orig) || ((proc->file->symtab + j)->st_value == position + sym_offset))) {
	  if ( DEBUG_COND )
	    kprintf("fixing symtab entry %d\n", j);
	  (proc->file->symtab + j)->st_value = kplt;
	}  
#endif
      /* increment table pointer */
      kplt += 20;

      /* otherwise treat it like any other section */
    }
    else
#endif
    sym->st_value = position + sym_offset;
  }

  /* Update our internal relocations to maybe have a new offset value.
   * NOTE: We are currently using r_offset as if our file is always
   *      considered an executable or shared library. This is a fairly safe
   *      assumption, but ELF allows for "relocatable files" that we might
   *      in theory want to load later on down the road. If that becomes the
   *      case, we'll need to change this. */
  for(i = 0; i < unit->num_rels; i++) {
    struct Elf64_Rel *rel = unit->rels + i;
    uint64_t rel_offset = rel->r_offset - unit->addr;
    rel->r_offset = position + rel_offset;
  }
  for(i = 0; i < unit->num_relas; i++) {
    struct Elf64_Rela *rela = unit->relas + i;
    uint64_t rela_offset = rela->r_offset - unit->addr;
    rela->r_offset = position + rela_offset;
  }

  /* Did our unit contain any important pointers, like RIP or RSP? If so,
   * update them now so they point to the correct places. */
  if(UNIT_CONTAINS(unit, ((Proc_t *)proc)->mc.rip))
    ((Proc_t *)proc)->mc.rip += position - unit->addr;
  if(UNIT_CONTAINS(unit, ((Proc_t *)proc)->mc.rsp))
    ((Proc_t *)proc)->mc.rsp += position - unit->addr;

  /* Update address. */
  unit->addr = position;

  return;
}

/******************************************************************************
 *
 * Function: fixup_unit(struct diversity_unit *, struct Elf64_Sym *)
 *
 * Description: Given a diversity unit, apply its external relocations. This
 *              needs to be done after all the diversity units have been moved,
 *              otherwise the offset fields in the relocations and the symbol
 *              values will have wonky and possibly incorrect numbers.
 *****************************************************************************/


static void fixup_unit(void *proc, struct diversity_unit *unit, struct Elf64_Sym *symtab) {

  struct external_reloc *reloc;
  struct Elf64_Sym *sym;
  int reltype;
  /* Relocation variables using the same names as in the ELF docs. All are
   * declared as 64-bit variables for maximum genericness. */
  int64_t A;
  uint64_t S;
  uint64_t P;
  /* Pointers to the data. We set them to NULL initially so the compiler won't
   * complain about unused variables. */
  uint8_t *rel8 = NULL;
  uint16_t *rel16 = NULL;
  uint32_t *rel32 = NULL;
  uint64_t *rel64 = NULL;

  /* Update the external relocations that point into this unit. */
  list_for_each(&unit->external_relocs, reloc, list) {

    /* This works regardless of the true type of the union. */
    sym = symtab + ELF64_R_SYM(reloc->rel->r_info);
    reltype = ELF64_R_TYPE(reloc->rel->r_info);
    S = sym->st_value;

    /* NOTE: Currently calculating P as if our file is always considered an
     *       executable. This is technically correct for now, but must be
     *       changed if we ever load "relocatable files" that aren't
     *       executable or shared objects. */
    P = reloc->rel->r_offset;

    if(reloc->type == RELA) {
      /* This is easy; the addend is explicit. */
      A = reloc->rela->r_addend;
    } else {
      /* The addend is the memory contents of the offset. */

      A = *(uint64_t *)P;

      /* We may have pulled in extra data depending on the size of the
       * qrelocation; cut it down if that's the case. */
      A >>= 64 - reloc_size(reltype);
    }

    /* We only support a select few relocation types at the moment, as we're
     * a pretty basic loader (no shared libraries, etc.). */
    switch(reltype) {
    case R_X86_64_64:
      rel64 = (uint64_t *)P;
      *rel64 = S + A;
      break;
    case R_X86_64_PC32:
      /* Ignore these relocations for now...we can't handle them, they
       * shouldn't even be generated in the first place (!), and their
       * presence doesn't do anything for us. At the moment, every single
       * one of these would be truncated anyway. */
      break;
    case R_X86_64_32:
    case R_X86_64_32S:
      rel32 = (uint32_t *)P;
      *rel32 = (uint32_t)(S + A);
      break;
    case R_X86_64_16:
      rel16 = (uint16_t *)P;
      *rel16 = (uint16_t)(S + A);
      break;
    case R_X86_64_8:
      rel8 = (uint8_t *)P;
      *rel8 = (uint8_t)(S + A);
      break;
    default:
      kprintf("Reached default statement in fixup_unit\n");
    }
  }
}

#define partition_size(p) (p->end - p->start)
#define unit_size(u) ( u->memsz + (u->hdr->sh_addralign - 1) )
#define new_unit_addr(u) ( (u->sign > 0) ? (u->addr + u->offset) : \
			                   (u->addr - u->offset))

static partition_t *partitions;
static partition_t *curr_partition;


static uint64_t n_parts;

void add_to_partitions(partition_t *parts, uint64_t start, uint64_t size) {

  partition_t *part, *new;

  for ( part = parts; part; part = part->next ) {
    if ( (part->start <= start) && (part->end >= start) ) {
      if ( (start + size) > part->end )
	part->end = (start + size) + (PAGE_SIZE - ((start+size)%PAGE_SIZE));
      return;
    }
    else if ( !part->next || (part->next->start > start) ) {

      /* make a new partition */
      new = (partition_t*)kmalloc_track(DIVERSITY_SITE, sizeof(partition_t));
      if ( !new ) {
	kputs("allocating partition failed in add_to_partitions");
	panic();
      }

      new->next = part->next;
      new->prev = part;
      part->next = new;
      if ( part->next )
	part->next->prev = new;

      new->start = start - (start % PAGE_SIZE);
      new->end   = (start + size) + (PAGE_SIZE - ((start + size) % PAGE_SIZE));

      return;
    }
  }

  kputs("Never should have made it here in add_to_partitions...");
  panic();

  /* never reached */
  return;
}

void split_partition(partition_t *part, uint64_t start, uint64_t end) {

  partition_t *new;

  if ( !part || !(part->end >= start) || !(part->start <= start) ||
       !(part->end >= end)   || !(part->start <= end)   ){
    kputs("ERROR: Tried to split a partition with invalid params.");
    if ( part )
      kprintf("part->start 0x%x, part->end 0x%x\n", part->start, part->end);
    else 
      kprintf("NULL PARTITION\n");
    kprintf("requested start 0x%x, requested end 0x%x\n", start, end);
    panic();
  }

  new = (partition_t*)kmalloc_track(DIVERSITY_SITE, sizeof(partition_t));
  if ( !new ) {
    kputs("Error allocating new partition");
    panic();
  }

#ifdef DIVERSITY_DEBUG
  kprintf("starting with partition \n      0x%x -> 0x%x\n",
	  part->start, part->end);
  kprintf("requested beginning and end of split is\n          0x%x -> 0x%x\n",
	  start, end);
#endif

  /* place the new partition after the existing partition */
  new->next = part->next;
  new->prev = part;
  if ( part->next )
    part->next->prev = new;
  part->next = new;

  /* start of new partition is the end of the last page of the unit */
  new->start = end;
  if ( end % PAGE_SIZE )
    new->start += PAGE_SIZE - (end % PAGE_SIZE);
  /* end of new partition is the end of the old partition we are splitting */
  new->end = part->end;

  /* adjust end of current partition to the first page unit will use */
  part->end = start - (start % PAGE_SIZE);

#ifdef DIVERSITY_DEBUG
  kprintf("ending with two partitions \n");
  kprintf("       0x%x -> 0x%x\n", part->start, part->end);
  kprintf("       0x%x -> 0x%x\n", new->start, new->end);
#endif

  /* we added a partition */
  n_parts++;

  return;
}

static void start_partition(uint64_t start) {

  partition_t *part;

#ifdef DIVERSITY_DEBUG
  kprintf("starting a partition @ 0x%x\n", start);
#endif

  if ( curr_partition ) {
    kputs("[DIVERSITY] Algorithm messed up partitions at start_partition");
    panic();
  }

  curr_partition = (partition_t*)kmalloc_track(DIVERSITY_SITE, 
					       sizeof(partition_t));
  if ( !curr_partition ) {
    kputs("[DIVERSITY] Allocation of a new partition failed in start_partition");
    panic();
  }
  kmemset(curr_partition, 0, sizeof(partition_t));

  for ( part = partitions; part && part->next; part = part->next ) ;

  curr_partition->prev = part;
  if ( part )
    part->next = curr_partition;

  curr_partition->start = start;

  if ( !partitions )
    partitions = curr_partition;

  n_parts++;

  return;
}

static void end_partition(uint64_t end) {

#ifdef DIVERSITY_DEBUG
  kprintf("ending partition @ 0x%x\n", end);
#endif

  if ( !curr_partition ) {
    kputs("[DIVERSITY] Algorithm messed up partitions at end_partition");
    panic();
  }

  curr_partition->end = end;

  curr_partition = 0x0;

  return;
}

void free_partitions(partition_t *parts) {

  partition_t *next;

  /* free the partitions */
  for ( ; parts; parts = next ) {
    next = parts->next;
    kfree_track(DIVERSITY_SITE, parts);
  }

  return;
}

partition_t *init_partitions(void) {

  int pml4t_idx, pdpt_idx, pd_idx, pt_idx;

  struct page_map_level_4_table *pml4t;
  struct page_directory_pointer_table *pdpt;
  struct page_directory *pd;
  struct page_table *pt;

#ifdef DIVERSITY_DEBUG
  kputs("In init partitions");
#endif

  partitions = 0x0;
  curr_partition = 0x0;

  /* loop through page tables */
  pml4t = (struct page_map_level_4_table*)phys2virt(read_cr3());
  for ( pml4t_idx = 0; pml4t_idx < 512; pml4t_idx++ ) {

    /* check for non-canonical addrs */
    if ( (pml4t_idx == 256) && curr_partition)
      end_partition((uint64_t)idx2vaddr(255,511,511,511));

    if ( !pml4t->entries[pml4t_idx].present ) {

      if ( !curr_partition ) 	/* if this begins a new partition */
	start_partition((uint64_t)idx2vaddr(pml4t_idx, 0, 0, 0));

      continue;
    }
    else if ( pml4t_idx == PML4T_RECURSIVE_ENTRY_IDX ) {
      if ( curr_partition )
	end_partition((uint64_t)idx2vaddr(pml4t_idx,0,0,0));
      continue;
    }

    pdpt = (struct page_directory_pointer_table*)PDPTE2vaddr(pml4t_idx,0);
    for ( pdpt_idx = 0; pdpt_idx < 512; pdpt_idx++ ) {

      if ( !pdpt->entries[pdpt_idx].present ) {

	if ( !curr_partition ) 	/* if this begins a new partition */
	  start_partition((uint64_t)idx2vaddr(pml4t_idx, pdpt_idx, 0, 0));

	continue;
      }

      pd = (struct page_directory*)PDE2vaddr(pml4t_idx,pdpt_idx,0);
      for ( pd_idx = 0; pd_idx < 512; pd_idx++ ) {

        if ( !pd->entries[pd_idx].present )  {

	  if ( !curr_partition ) 	/* if this begins a new partition */
	    start_partition((uint64_t)idx2vaddr(pml4t_idx, pdpt_idx, pd_idx, 0));

	  continue;
	}

	pt = (struct page_table*)PTE2vaddr(pml4t_idx, pdpt_idx, pd_idx, 0);
        for ( pt_idx = 0; pt_idx < 512; pt_idx++ ) {

          if ( !pt->entries[pt_idx].present ) {

	    if ( !curr_partition )        /* if this begins a new partition */
	      start_partition((uint64_t)idx2vaddr(pml4t_idx, pdpt_idx,
						  pd_idx, pt_idx));

	    continue;
	  }
	  else /* the page is present */
	    if ( curr_partition ) /* we found the end of the partition */
	      end_partition((uint64_t)idx2vaddr(pml4t_idx, pdpt_idx,
						pd_idx, pt_idx));

        } /* end pt level loop */
      } /* end pd level loop */
    } /* end pdpt level loop */
  } /* end pml4t level loop */

#ifdef DIVERSITY_DEBUG
  kputs("End of init partitions. Here is the state of the linked list");
  for ( curr_partition = partitions; curr_partition; curr_partition = curr_partition->next )
    kprintf("  Node: start @ 0x%x, end @ 0x%x\n", curr_partition->start, curr_partition->end);
#endif

  return partitions;
}

uint64_t pick_random_place(partition_t *parts, uint64_t size,
			   uint64_t align, partition_t **p) {

  uint64_t rand;
  uint64_t a = 0x0;
  partition_t *part;

  for ( part = parts; part; part = part->next ) {
    if ( size + align > part->end - part->start )
      continue;
    else
      a += (part->end - part->start - size - align);
  }

  rand = random_between_align(0, a, align);

  /* this function will work on units of alignment */
  rand /= align;

#ifdef DIVERSITY_DEBUG
  kputs("\nIn pick_random_place");
  kprintf("   size = 0x%x, alignment = 0x%x\n", size, align);
  kprintf("   available space = 0x%x\n", a);
  kprintf("   rand chosen = 0x%x\n", rand);
#endif

  for ( part = parts; part; part = part->next ) {
    if ( size + align > part->end - part->start)
      continue;
    else if ( (part->end - part->start - size - align)/align <= rand )
      rand -= ( part->end - part->start - size - align )/align;
    else {
#ifdef DIVERSITY_DEBUG
      kprintf("   return partition with start 0x%x, end 0x%x\n",
	      part->start, part->end);
      kprintf("   random offset remaining: 0x%x\n", rand);
      kprintf("   address returned: 0x%x\n", part->start + (rand*align));
#endif
      *p = part;
      return part->start + (rand*align);
    }
  }

  *p = 0x0;
  return 0xAAAAAAAAAAAAAAA;
}


/******************************************************************************
 *
 * Function: diversify(Proc_t *)
 *
 * Description: Given a process, diversify it.
 * Side effects: Process memory is changed; the metadata in the ELF context is
 *               updated accordingly.
 *
 *****************************************************************************/
void diversify(void *generic_proc, SHA256_CTX *ctx) {

  Proc_t *proc = (Proc_t *)generic_proc;
  struct diversity_unit **units;
  struct diversity_unit *unit;
  uint64_t position;
  uint64_t i;
  partition_t *partition, *next;
  char *position_char;

  /* generate and initialize the list of diversity units */
  units = kmalloc_track(DIVERSITY_SITE, sizeof(struct diversity_unit *) * proc->file->num_diversity_units);
  kmemset(units, 0, sizeof(struct diversity_unit *) * proc->file->num_diversity_units);

  i = 0;
  list_for_each(&proc->file->diversity_units, unit, list) {
    *(units + i++) = unit;
  }

  /* initialize the list of partitions */
  partitions = init_partitions();

  /* cover the hypervisor's stupid ass. */
  for ( partition = partitions; partition; partition = partition->next ){
    if ( (partition->start <= 0x100000000000) &&
	 (partition->end   >= 0x300000000000) ) {
      split_partition(partition, 0x100000000000, 0x300000000000);
      break;
    }
  }
  for ( partition = partitions; partition; partition = partition->next ){
    if ( (partition->start <= 0x100000) &&
	 (partition->end   >= 0x1000000) ) {
      split_partition(partitions, 0x100000, 0x1000000);
      break;
    }
  }
#if defined(KPLT) && defined(BOOTLOADER)
  /* place the kplt! */
  kplt = pick_random_place(partitions, 3*PAGE_SIZE, PAGE_SIZE, &partition);
  split_partition(partition, kplt, kplt + (3*PAGE_SIZE));

  /* this line will put the kplt into a known location */
  //kplt = 0x70000000000;

  vmem_alloc((uint64_t *)kplt, 3*PAGE_SIZE, PG_RW);

  /* return the kplt pointer */
  proc->mc.r13 = kplt;
#endif

  /* diversity algorithm:
     -------------------------------

   */

  /* for each unit */
  for(i = 0; i < proc->file->num_diversity_units; i++) {
    unit = *(units + i);

    position = pick_random_place(partitions, unit_size(unit),
				 unit->hdr->sh_addralign, &partition);

    /* split the position into two: make current partition small and then
       add a new partition for the second part. Note: partitions will be
       aligned by page boundaries to assist in the virtual memory system.
    */
    split_partition(partition, position, position + unit->memsz);

    /*
       IMPORTANT: Uncomment this in order to do the entire diversity algorithm
          but still load all sections in the place that they belong when
	  undiversified. This helps in debugging...
    */
    //position = unit->addr;

    /* store offset for unit for later use */
    if ( position <= unit->addr ) {
      unit->sign = -1;
      unit->offset = unit->addr - position;
    }
    else {
      unit->sign = 1;
      unit->offset = position - unit->addr;
    }

    /* Relocate current diversity unit to position. */
    position_char = kitoa(position);
    sha256_update(ctx, (unsigned char*)position_char,
		  kstrlen(position_char)+1);
    kfree_track(DIVERSITY_SITE, position_char);
  }

  /* free the partitions */
  for ( partition = partitions; partition; partition = next ) {
    next = partition->next;
    kfree_track(DIVERSITY_SITE, partition);
  }

  list_for_each(&proc->file->diversity_units, unit, list) {

    if(UNIT_CONTAINS(unit, ((Proc_t *)proc)->mc.rsp)) {

#ifdef KERNEL

      vmem_alloc((uint64_t*)new_unit_addr(unit),
		 unit->memsz, PG_RW|PG_USER|PG_NX );
      add_memory_region( proc, STACK_REGION, USR_STACK_PERMS,
                         new_unit_addr(unit),
			 new_unit_addr(unit) + unit->memsz );
#else /* Bootloader */

      vmem_alloc( (uint64_t*)new_unit_addr(unit), unit->memsz,
		  PG_RW|PG_NX );
#endif
    }
#ifdef KERNEL
    else if( UNIT_CONTAINS(unit, 0x4F000000000) ) {

      /* heap */
      add_memory_region( proc, HEAP_REGION, PG_USER | PG_RW | PG_NX,
			 new_unit_addr(unit), new_unit_addr(unit) );

      //kprintf("[Kernel] User heap @ 0x%x\n", new_unit_addr(unit));

    }
#endif
    else if ( kstreq(ELF_SECNAME(*unit->hdr, proc->file->section_strtab),
		     ".bss") ) {
#ifdef KERNEL

      add_memory_region( proc, BSS_REGION, PG_USER | PG_RW | PG_NX,
			 new_unit_addr(unit),
			 new_unit_addr(unit) + unit->memsz );

      vmem_alloc( (uint64_t*)new_unit_addr(unit), unit->memsz,
		  PG_RW|PG_USER|PG_NX );
#else /* Bootloader */
      vmem_alloc( (uint64_t*)new_unit_addr(unit), unit->memsz,
		  PG_RW|PG_GLOBAL|PG_NX );
#endif

      kmemset( (void*)new_unit_addr(unit),  0, unit->memsz );
    }
    else {

#ifdef KERNEL
      add_memory_region( proc, TEXT_REGION, USR_TEXT_PERMS,
                         new_unit_addr(unit),
			 new_unit_addr(unit) + unit->memsz );

      vmem_alloc( (uint64_t*)new_unit_addr(unit), unit->memsz,
		  PG_RW | PG_USER );
#else /* Bootloader */

      vmem_alloc( (uint64_t*)new_unit_addr(unit), unit->memsz,
		  PG_RW | PG_GLOBAL );

#endif

      /* read from the file into the diversified virtual memory location */
      if ( proc->is_in_mem ) {
	mem_seek( proc->file->file_ctx, 0 );
	mem_seek( proc->file->file_ctx, unit->hdr->sh_offset );
	mem_read( proc->file->file_ctx, (void*)new_unit_addr(unit), unit->memsz );
      }
      else {
	file_seek( proc->file->file_ctx, 0 );
	file_seek( proc->file->file_ctx, unit->hdr->sh_offset );
	file_read( proc->file->file_ctx, (void*)new_unit_addr(unit), unit->memsz );
      }
    }
  }

  list_for_each(&proc->file->diversity_units, unit, list) {
    move_unit(proc, unit, new_unit_addr(unit) );
  }

  /* No longer need the units in array form, so release the structure. */
  kfree_track(DIVERSITY_SITE,units);

  /* Now that all the units have been moved, we can apply the relocations. */
  list_for_each(&proc->file->diversity_units, unit, list) {
    fixup_unit(proc, unit, proc->file->symtab);
  }
#ifndef KERNEL
  /* remove write permissions for global data */
  list_for_each(&proc->file->diversity_units, unit, list) {

    if(UNIT_CONTAINS(unit, ((Proc_t *)proc)->mc.rsp)) {
      ;
    }
    else if ( kstreq(ELF_SECNAME(*unit->hdr, proc->file->section_strtab),
		     ".bss") ) {
      ;
    }
    else if ( kstreq(ELF_SECNAME(*unit->hdr, proc->file->section_strtab),
		     ".data") ) {
      ;
    }
    else if ( kstreq(ELF_SECNAME(*unit->hdr, proc->file->section_strtab),
		     ".rodata") ) {
      set_page_permission(unit->addr, unit->memsz, PG_GLOBAL | PG_RW);
    }
    else {
      set_page_permission(unit->addr, unit->memsz, PG_GLOBAL);
    }
  }
#endif

  return;
}

static char* kitoa(uint64_t val) {
  char *res;
  int idx;

  if((res=(char*)kmalloc_track(DIVERSITY_SITE, 32*sizeof(char)))==NULL)
    return NULL;

  kmemset(res, '\0', 32);

  idx=0;
  while(val) {
    res[idx++] = (val%10) + '\0';
    val /= 10;
  }

  return res;
}

