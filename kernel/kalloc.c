// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct {
  struct spinlock lock;
  int cnt[(PHYSTOP-KERNBASE)/PGSIZE];
} ref;

void
ref_cnt_inc(void * pa) {
  acquire(&ref.lock);
  ref.cnt[REFIND(pa)] += 1;
  release(&ref.lock);
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&ref.lock, "ref");

  acquire(&ref.lock);
  for(int i = 0; i < (PHYSTOP-KERNBASE)/PGSIZE; i++) {
    ref.cnt[i] = 0;
  }
  release(&ref.lock);

  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&ref.lock);
  int ref_index = REFIND((uint64)pa);
  if (ref.cnt[ref_index] > 1) {
    ref.cnt[ref_index] -= 1;
    release(&ref.lock);
    return;
  }
  release(&ref.lock);

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    acquire(&ref.lock);
    ref.cnt[REFIND(r)] = 1;
    release(&ref.lock);
  }
  return (void*)r;
}

uint64
cow_handler(pagetable_t pagetable, uint64 va) { 
  va = PGROUNDDOWN(va); 
  if (va >= MAXVA) return -1;
  pte_t * pte = walk(pagetable, va, 0);
  if (pte == 0) return -1;
  if ((*pte & PTE_V) == 0) {
    return -1;
  }
  if ((*pte & PTE_U) == 0) {
    return -1;
  }
  if ((*pte & PTE_C) == 0) return 0;

  uint64 pa = PTE2PA(*pte);
  if (pa == 0) return -1;
  uint flags = (PTE_FLAGS(*pte) & ~PTE_C) | PTE_W;

  char * mem;
  if ((mem = kalloc()) == 0) {
    return -1;
  }
  memmove(mem, (char*)pa, PGSIZE);
  *pte = (PA2PTE((uint64)mem) & ~PTE_C ) | flags;// redirect pte to point to the new pa
  kfree((void*)pa);
  return 0;
}
