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
int references[((PHYSTOP-KERNBASE)/PGSIZE)+1];

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
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

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
  
  uint64 start = PGROUNDUP((uint64)end);
  int index = ((uint64)pa - start )/ PGSIZE;
  if ( references[index] > 1 ) {
    references[index]--;
    return;
  }
  references[index] = 0;

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

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  if ( (uint64)r != 0 ) {
    uint64 start = PGROUNDUP((uint64)end);
    int index = ((uint64)r - start )/ PGSIZE;
    references[index] = 1;
  }
  return (void*)r;
}


void kincrement(uint64 pa) {
  if((pa % PGSIZE) != 0 || (char*)pa < end || pa >= PHYSTOP)
    panic("kincrement");

  uint64 start = PGROUNDUP((uint64)end);
  int index = (pa - start )/ PGSIZE;
  references[index]++;
}

// Get free memory size
int
kfreememsize(void)
{
  struct run *r;
  int count;
  acquire(&kmem.lock);
  r = kmem.freelist;
  count = 0;
  while(r) {
    count++;
    r = r->next;
  }
  release(&kmem.lock);
  return count*PGSIZE;
}
