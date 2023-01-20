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
  struct spinlock locks[NCPU];
  struct run *freelists[NCPU];
} kmem;

int
cpu_id()
{
  int cpu_id;
  push_off();
  cpu_id = cpuid();
  pop_off();
  return cpu_id;
}

void
kinit()
{
  for ( int i = 0; i < NCPU; i++ ) {
    char name[7];
    snprintf(name, 7, "kmem %d", i);
    initlock(&kmem.locks[i], name);
  }
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
  int id;

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
  id = cpu_id();

  acquire(&kmem.locks[id]);
  r->next = kmem.freelists[id];
  kmem.freelists[id] = r;
  release(&kmem.locks[id]);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  int id;

  id = cpu_id();
  acquire(&kmem.locks[id]);
  r = kmem.freelists[id];
  if(r) {
      kmem.freelists[id] = r->next;
      release(&kmem.locks[id]);
  }
  else {
    release(&kmem.locks[id]);
    // Steal from other cpu
    for ( int i = 0; i < NCPU; i++ ) {
      if ( i == id )
        continue;
      acquire(&kmem.locks[i]);
      r = kmem.freelists[i];
      if (r) {
        kmem.freelists[i] = r->next;
        release(&kmem.locks[i]);
        break;
      }
      else {
        release(&kmem.locks[i]);
      }
    }
  }

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
