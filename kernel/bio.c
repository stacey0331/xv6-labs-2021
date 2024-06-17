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

#define NBUCKET 13
#define hash(blockno) (blockno) % NBUCKET

struct {
  struct buf buf[NBUF];

  struct spinlock misslock;

  struct spinlock lock[NBUCKET];
  // // Linked list of all buffers, through prev/next.
  // // Sorted by how recently the buffer was used.
  // // head.next is most recent, head.prev is least.
  struct buf head[NBUCKET];
} bcache;

void
binit(void)
{
  initlock(&bcache.misslock, "bcache");

  for (int i = 0; i < NBUCKET; i++) {
    initlock(&bcache.lock[i], "bcache");
    bcache.head[i].next = &bcache.head[i];
  }

  // put buf into different lists 
  for (int i = 0; i < NBUF; i++) {
    initsleeplock(&bcache.buf[i].lock, "buffer");
    bcache.buf[i].next = bcache.head[i%NBUCKET].next;
    bcache.head[i%NBUCKET].next = &bcache.buf[i];
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf * b;
  struct buf * newb = 0;
  uint idx = hash(blockno);
  uint min_time = __UINT32_MAX__;

  acquire(&bcache.lock[idx]);
  // Is the block already cached?
  for(b = bcache.head[idx].next; b != &bcache.head[idx]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[idx]);
      acquiresleep(&b->lock);
      return b;
    }

    if (b->refcnt == 0 && (b->time < min_time)) {
      min_time = b->time;
      newb = b;
    }
  }
  if ((b=newb)) {
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    release(&bcache.lock[idx]);
    acquiresleep(&b->lock);
    return b;
  }
  release(&bcache.lock[idx]);

  // no free block in current bucket, get from other buckes
  acquire(&bcache.misslock);
  // if 2 processes try to get same block this prevents double cache
  for (b = bcache.buf; b < bcache.buf+NBUF; b++) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.misslock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  struct buf * prevb;
  struct buf * prev;
  int checkidx;
  for (int i = 1; i < NBUCKET; i++) {
    checkidx = (idx + i) % NBUCKET;
    min_time = __UINT32_MAX__;
    acquire(&bcache.lock[checkidx]);
    prev = &bcache.head[checkidx];
    for(b = bcache.head[checkidx].next; b != &bcache.head[checkidx]; b = b->next){
      if (b->refcnt == 0 && (b->time < min_time)) {
        min_time = b->time;
        newb = b;
        prevb = prev;
      }
      prev = b;
    }
    
    if (newb) {
      break;
    }

    release(&bcache.lock[checkidx]);
  }

  if (!(b=newb)) {
    panic("bget: no buffers");
    return 0;
  }

  // remove from old bucket
  prevb->next = newb->next;
  release(&bcache.lock[checkidx]);

  // set fields
  b->dev = dev;
  b->blockno = blockno;
  b->valid = 0;
  b->refcnt = 1;
  release(&bcache.misslock);

  // add to this bucket
  acquire(&bcache.lock[idx]);
  b->next = bcache.head[idx].next;
  bcache.head[idx].next = b;
  release(&bcache.lock[idx]);
  
  acquiresleep(&b->lock);
  return b;
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

  uint idx = hash(b->blockno);

  acquire(&bcache.lock[idx]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    acquire(&tickslock);
    b->time = ticks;
    release(&tickslock);
  }
  release(&bcache.lock[idx]);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock[hash(b->blockno)]);
  b->refcnt++;
  release(&bcache.lock[hash(b->blockno)]);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock[hash(b->blockno)]);
  b->refcnt--;
  release(&bcache.lock[hash(b->blockno)]);
}


