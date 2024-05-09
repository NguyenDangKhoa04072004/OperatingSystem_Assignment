//#ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Memory management unit mm/mm.c
 */

#include "mm.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
/* 
 * init_pte - Initialize PTE entry
 */
int init_pte(uint32_t *pte,
             int pre,    // present
             int fpn,    // FPN
             int drt,    // dirty
             int swp,    // swap
             int swptyp, // swap type
             int swpoff) //swap offset
{
    if (swp == 0) { // Non swap ~ page online
      // if (fpn == 0) 
      //   return -1; // Invalid setting

      /* Valid setting with FPN */
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);
      SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT); 
    } else { // page swapped
      CLRBIT(*pte, PAGING_PTE_PRESENT_MASK);
      SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);
      SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT); 
      SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
    }

  return 0;   
}

/* 
 * pte_set_swap - Set PTE entry for swapped page
 * @pte    : target page table entry (PTE)
 * @swptyp : swap type
 * @swpoff : swap offset
 */
int pte_set_swap(uint32_t *pte, int swptyp, int swpoff)
{
  // SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  CLRBIT(*pte,PAGING_PTE_PRESENT_MASK);
  SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
  SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
  SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);

  return 0;
}

/* 
 * pte_set_swap - Set PTE entry for on-line page
 * @pte   : target page table entry (PTE)
 * @fpn   : frame page number (FPN)
 */
int pte_set_fpn(uint32_t *pte, int fpn)
{
  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
  SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT); 

  return 0;
}


/* 
 * vmap_page_range - map a range of page at aligned address
 */
int vmap_page_range(struct pcb_t *caller, // process call
                                int addr, // start address which is aligned to pagesz
                               int pgnum, // num of mapping page
                               int swap,
           struct framephy_struct *frames,// list of the mapped frames
              struct vm_rg_struct *ret_rg)// return mapped region, the real mapped fp
{                                         // no guarantee all given pages are mapped
  //int  fpn;
  ret_rg->rg_end = ret_rg->rg_start = addr; // at least the very first space is usable

  /* TODO map range of frame to address space 
   *      [addr to addr + pgnum*PAGING_PAGESZ
   *      in page table caller->mm->pgd[]
   */
    
    if(swap == 0){
     uint32_t * pte = malloc(sizeof(uint32_t));
     int pgn = PAGING_PGN(addr);
     init_pte(pte,1,frames->fpn,0,0,0,0);
     caller->mm->pgd[pgn] = *pte;
     enlist_pgn_node(&caller->mm->fifo_pgn, pgn);
    }else{
     uint32_t * pte = malloc(sizeof(uint32_t));
     int pgn = PAGING_PGN(addr);
     init_pte(pte,1,0,0,1,0,frames->fpn);
     caller->mm->pgd[pgn] = *pte;
     enlist_pgn_node(&caller->mm->fifo_pgn, pgn);
    }
    
   /* Tracking for later page replacement activities (if needed)
    * Enqueue new usage page */
  


  return 0;
}
int swap_out_pages(struct pcb_t *caller, // process call
                               int pgnum, // num of mapping page
           struct framephy_struct *frames// list of the mapped frames
           )
{   
    if(frames == NULL){
      return -1;
    }
      int pgn = -1;
      struct framephy_struct * freeframe = malloc(sizeof(struct framephy_struct));
      find_victim_page(caller->mm,&pgn);
      __swap_cp_page( caller->mram,PAGING_FPN_PRESENT(caller->mm->pgd[pgn]),caller->active_mswp,frames->fpn);
      freeframe->fpn = PAGING_FPN_PRESENT(caller->mm->pgd[pgn]);
      pte_set_swap(&(caller->mm->pgd[pgn]),0,frames->fpn);
      freeframe->owner = caller->mm;
      freeframe->fp_next = caller->mram->free_fp_list;
      caller->mram->free_fp_list =  freeframe;
      return 0;
}
/* 
 * alloc_pages_range - allocate req_pgnum of frame in ram
 * @caller    : caller
 * @req_pgnum : request page num
 * @frm_lst   : frame list
 */

int alloc_pages_range(struct pcb_t *caller, int req_pgnum, struct framephy_struct** alloc_frame)
{
  int fpn;
  if(MEMPHY_get_freefp(caller->mram, &fpn) == -1){
      return -1;
  }
  (*alloc_frame) = malloc(sizeof(struct framephy_struct));
  (*alloc_frame)->fpn = fpn;
  (*alloc_frame)->owner = caller->mm;
  (*alloc_frame)->fp_next= NULL;
  return 0;
}

int alloc_swap_pages(struct pcb_t * caller, int req_pgnum, struct framephy_struct** alloc_frame){
  /*
     Get frame from swap space
     swap victim pages out RAM
     swap allocated pages in RAM
  */
  int offset;
  if(MEMPHY_get_swap_page(caller->active_mswp,&offset) == -1){
    return -1;
  }
  (*alloc_frame) = malloc(sizeof(struct framephy_struct));
  (*alloc_frame)->fpn = offset;
  (*alloc_frame)->owner = caller->mm;
  (*alloc_frame)->fp_next= NULL;
  return 0;
}
/* 
 * vm_map_ram - do the mapping all vm are to ram storage device
 * @caller    : caller
 * @astart    : vm area start
 * @aend      : vm area end
 * @mapstart  : start mapping point
 * @incpgnum  : number of mapped page
 * @ret_rg    : returned region
 */
int vm_map_ram(struct pcb_t *caller, int astart, int aend, int mapstart, int incpgnum, struct vm_rg_struct *ret_rg)
{
  /*@bksysnet: author provides a feasible solution of getting frames
   *FATAL logic in here, wrong behaviour if we have not enough page
   *i.e. we request 1000 frames meanwhile our RAM has size of 3 frames
   *Don't try to perform that case in this simple work, it will result
   *in endless procedure of swap-off to get frame and we have not provide 
   *duplicate control mechanism, keep it simple
   */
  for(int i = 0 ; i < incpgnum; i++, mapstart+= PAGING_PAGESZ){
      struct framephy_struct * alloc_frame = NULL;
      struct framephy_struct * swap_frame = NULL;
      if(alloc_pages_range(caller, incpgnum, &alloc_frame) == 0){
         vmap_page_range(caller, mapstart,incpgnum,0, alloc_frame, ret_rg);
      }else if(alloc_swap_pages(caller,incpgnum, &swap_frame) == 0){
         vmap_page_range(caller, mapstart,incpgnum,1,swap_frame, ret_rg);
      }else{
         printf("OOM: vm_map_ram out of memory \n");
         return -1;
      }
  }


  // if(incpgnum <= mram_align_size/PAGING_PAGESZ){
  //    if(alloc_pages_range(caller, incpgnum, &frm_lst) == 0 && frm_lst != NULL){
  //      vmap_page_range(caller, mapstart,incpgnum, frm_lst, ret_rg);
  //   }else if(alloc_swap_pages(caller,incpgnum, &frm_lst) == 0){
  //       swap_out_pages(caller, incpgnum, frm_lst);
  //       vm_map_ram(caller,astart,aend,mapstart,incpgnum,ret_rg);
  //   }else{
  //       printf("OOM: vm_map_ram out of memory \n");
  //       return -1;
  //   }
  // }else{
  //    return -1;
  // }
  
  /* Out of memory */
//   if (ret_alloc == -3000) 
//   {
    
// #ifdef MMDBG
//      printf("OOM: vm_map_ram out of memory \n");
  
// #endif
//      return -1;
//   }

  /* it leaves the case of memory is enough but half in ram, half in swap
   * do the swaping all to swapper to get the all in ram */
  
  

  return 0;
}

/* Swap copy content page from source frame to destination frame 
 * @mpsrc  : source memphy
 * @srcfpn : source physical page number (FPN)
 * @mpdst  : destination memphy
 * @dstfpn : destination physical page number (FPN)
 **/
int __swap_cp_page(struct memphy_struct *mpsrc, int srcfpn,
                struct memphy_struct *mpdst, int dstfpn) 
{
  int cellidx;
  int addrsrc,addrdst;
  for(cellidx = 0; cellidx < PAGING_PAGESZ; cellidx++)
  {
    addrsrc = srcfpn * PAGING_PAGESZ + cellidx;
    addrdst = dstfpn * PAGING_PAGESZ + cellidx;

    BYTE data;
    MEMPHY_read(mpsrc, addrsrc, &data);
    MEMPHY_write(mpdst, addrdst, data);
  }

  return 0;
}

int _cache_page(struct memphy_struct *mpsrc, int srcfpn,
                struct memphy_struct *mpdst, int dstfpn){
  int cellidx;
  int addrsrc,addrdst;
  for(cellidx = 0; cellidx < PAGING_PAGESZ; cellidx++)
  {
    addrsrc = srcfpn * PAGING_PAGESZ + cellidx;
    addrdst = dstfpn * PAGING_PAGESZ + cellidx;

    BYTE data;
    MEMPHY_read(mpsrc, addrsrc, &data);
    MEMPHY_write(mpdst, addrdst, data);
  }
  return 0;
}
/*
 *Initialize a empty Memory Management instance
 * @mm:     self mm
 * @caller: mm owner
 */
int init_mm(struct mm_struct *mm, struct pcb_t *caller)
{
  struct vm_area_struct * vma = malloc(sizeof(struct vm_area_struct));

  mm->pgd = malloc(PAGING_MAX_PGN*sizeof(uint32_t));

  /* By default the owner comes with at least one vma */
  vma->vm_id = 0;
  vma->vm_start = 0;
  vma->vm_end = vma->vm_start;
  vma->sbrk = vma->vm_start;
  struct vm_rg_struct *first_rg = init_vm_rg(vma->vm_start, vma->vm_end);
  vma->previous_region = NULL;
  enlist_vm_rg_node(&vma->vm_rg_list, first_rg);

  vma->vm_next = NULL;
  vma->vm_mm = mm; /*point back to vma owner */

  mm->mmap = vma;

  return 0;
}

struct vm_rg_struct* init_vm_rg(int rg_start, int rg_end)
{
  struct vm_rg_struct *rgnode = malloc(sizeof(struct vm_rg_struct));

  rgnode->rg_start = rg_start;
  rgnode->rg_end = rg_end;
  rgnode->rg_next = NULL;

  return rgnode;
}

int enlist_vm_rg_node(struct vm_rg_struct **rglist, struct vm_rg_struct* rgnode)
{
  rgnode->rg_next = *rglist;
  *rglist = rgnode;
  return 0;
}

int enlist_pgn_node(struct pgn_t **plist, int pgn)
{
  struct pgn_t* pnode = malloc(sizeof(struct pgn_t));
  pnode->pgn = pgn;
  pnode->pg_next = NULL;
  if(*plist == NULL){
    *plist = pnode;
  }else{
    struct pgn_t * pgit= *plist;
    while (pgit->pg_next != NULL)
    {
      pgit = pgit->pg_next;
    }
    pgit->pg_next = pnode;
  }
  return 0;
}

int print_list_fp(struct framephy_struct *ifp)
{
   struct framephy_struct *fp = ifp;
 
   printf("print_list_fp: ");
   if (fp == NULL) {printf("NULL list\n"); return -1;}
   printf("\n");
   while (fp != NULL )
   {
       printf("fp[%d]\n",fp->fpn);
       fp = fp->fp_next;
   }
   printf("\n");
   return 0;
}

int print_list_rg(struct vm_rg_struct *irg)
{
   struct vm_rg_struct *rg = irg;
 
   printf("print_list_rg: ");
   if (rg == NULL) {printf("NULL list\n"); return -1;}
   printf("\n");
   while (rg != NULL)
   {
       printf("rg[%ld->%ld]\n",rg->rg_start, rg->rg_end);
       rg = rg->rg_next;
   }
   printf("\n");
   return 0;
}

int print_list_vma(struct vm_area_struct *ivma)
{
   struct vm_area_struct *vma = ivma;
 
   printf("print_list_vma: ");
   if (vma == NULL) {printf("NULL list\n"); return -1;}
   printf("\n");
   while (vma != NULL )
   {
       printf("va[%ld->%ld]\n",vma->vm_start, vma->vm_end);
       vma = vma->vm_next;
   }
   printf("\n");
   return 0;
}

int print_list_pgn(struct pgn_t *ip)
{
   printf("print_list_pgn: ");
   if (ip == NULL) {printf("NULL list\n"); return -1;}
   printf("\n");
   while (ip != NULL )
   {
       printf("va[%d]-\n",ip->pgn);
       ip = ip->pg_next;
   }
   printf("n");
   return 0;
}

int print_pgtbl(struct pcb_t *caller, uint32_t start, uint32_t end)
{
  int pgn_start,pgn_end;
  int pgit;

  if(end == -1){
    pgn_start = 0;
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, 0);
    end = cur_vma->vm_end;
  }
  pgn_start = PAGING_PGN(start);
  pgn_end = PAGING_PGN(end);

  printf("print_pgtbl: %d - %d - pid: %d", start, end, caller->pid);
  if (caller == NULL) {printf("NULL caller\n"); return -1;}
    printf("\n");


  for(pgit = pgn_start; pgit < pgn_end; pgit++)
  {
     printf("%08ld: %08x\n", pgit * sizeof(uint32_t), caller->mm->pgd[pgit]);
  }

  return 0;
}

//#endif
