// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct {
  struct spinlock locks[NBUFBUCKET];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf heads[NBUFBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;
  int i, index;

  for ( i = 0; i < NBUFBUCKET; i++ ) {
    char name[10];
    snprintf(name, 10, "bcache %d", i);
    initlock(&bcache.locks[i], name);
    // Create linked list of buffers
    bcache.heads[i].prev = &bcache.heads[i];
    bcache.heads[i].next = &bcache.heads[i];
  }

  
  for(b = bcache.buf, i = 0; b < bcache.buf+NBUF; b++, i++){
    index = i % NBUFBUCKET;
    b->next = bcache.heads[index].next;
    b->prev = &bcache.heads[index];
    initsleeplock(&b->lock, "buffer");
    bcache.heads[index].next->prev = b;
    bcache.heads[index].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int index = blockno % NBUFBUCKET;

  acquire(&bcache.locks[index]);

  // Is the block already cached?
  for(b = bcache.heads[index].next; b != &bcache.heads[index]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.locks[index]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  int i = index;
  // Not cached.
  // Check all the buckets, remove block from old bucket, add new block in indexed bucket
  do {
      if ( i != index ) {
        acquire(&bcache.locks[i]);
      }
      for(b = bcache.heads[i].next; b != &bcache.heads[i]; b = b->next){
        if(b->refcnt == 0) {
          b->dev = dev;
          b->blockno = blockno;
          b->valid = 0;
          b->refcnt = 1;
          if ( i != index ) {
            b->next->prev = b->prev;
            b->prev->next = b->next;
            release(&bcache.locks[i]);
            acquire(&bcache.locks[index]);
            b->next = bcache.heads[index].next;
            b->prev = &bcache.heads[index];
            bcache.heads[index].next->prev = b;
            bcache.heads[index].next = b;
            release(&bcache.locks[index]);
          }
          else {
            release(&bcache.locks[i]);
          }
          acquiresleep(&b->lock);
          return b;
        }
      }
      release(&bcache.locks[i]);
      i++;
      i = i%NBUFBUCKET;
  }
  while ( i != index );
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&bcache.locks[b->blockno%NBUFBUCKET]);
  b->refcnt--;
  release(&bcache.locks[b->blockno%NBUFBUCKET]);
}

void
bpin(struct buf *b) {
  acquire(&bcache.locks[b->blockno%NBUFBUCKET]);
  b->refcnt++;
  release(&bcache.locks[b->blockno%NBUFBUCKET]);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.locks[b->blockno%NBUFBUCKET]);
  b->refcnt--;
  release(&bcache.locks[b->blockno%NBUFBUCKET]);
}


