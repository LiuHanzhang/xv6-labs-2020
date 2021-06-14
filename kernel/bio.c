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

/********************
Basic Idea: https://piazza.com/class/kgkxf1hjf3kw7?cid=59
TODO: Possibly might fail bcachetest. Still don't know why.
*********************/

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"
#include <limits.h>

#define NBUCKET 13

extern uint ticks;

struct {
  // NOTE: This lock is no longer used to protect the doubly linked list structure. 
  // Now it serves as a global lock that whoever wants to steal from other buckets must hold.
  struct spinlock lock;
  struct buf buf[NBUF];

  struct buf *hashtable[NBUCKET];
  struct spinlock bucketlock[NBUCKET]; // NOTE: The original bcache.lock is split into bucketlocks.
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache-global");

  for(int i = 0; i < NBUCKET; i++){
    initlock(bcache.bucketlock + i, "bcache-bucket"); // init locks per bucket
    bcache.hashtable[i] = bcache.buf + i; // and put the first buf in each bucket
    initsleeplock(&bcache.hashtable[i]->lock, "buffer"); // and init buffer locks
  }

  for(int i = NBUCKET; i < NBUF; i++){ // put each buffer cache evenly in each bucket
    b = bcache.hashtable[i % NBUCKET];
    while(b->next)
      b = b->next;
    initsleeplock(&(bcache.buf + i)->lock, "buffer");
    b->next = bcache.buf + i;
  }
}

// Hashtable Scan for LRU
// A utility function for bget()
// Scan through the linked list of buffers on one bucket.
// Given the head node of the linked list, return the least recently used unoccupied buffer, if any,
// and modify the second parameter to the addr of lru's previous buffer, if it isn't 0.
struct buf*
hslru(struct buf* b, struct buf **pprev)
{
  struct buf *lru = 0, *prev = 0;
  uint min_timestamp = UINT_MAX;
  for(; b != 0; b = b->next){
    if(b->timestamp < min_timestamp && b->refcnt == 0){
      lru = b;
      if(pprev)
        *pprev = prev;
      min_timestamp = b->timestamp;
    }
    prev = b;
  }
  return lru;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint bucketno = blockno % NBUCKET;
  acquire(bcache.bucketlock + bucketno);

  // Stage 1: Is the block already cached?
  for(b = bcache.hashtable[bucketno]; b != 0; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(bcache.bucketlock + bucketno);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Stage 2: Not cached.
  // Recycle the least recently used (LRU) unused buffer based on the timestamp.
  struct buf *lru = hslru(bcache.hashtable[bucketno], 0);
  if(lru){
    lru->dev = dev;
    lru->blockno = blockno;
    lru->valid = 0;
    lru->refcnt = 1;
    release(bcache.bucketlock + bucketno);
    acquiresleep(&lru->lock);
    return lru;
  }

  // Stage 3: No buffer to be recycled.
  // Steal buffer from other buckets
  release(bcache.bucketlock + bucketno); // release bucket lock
  acquire(&bcache.lock); // acquire the bcache global lock before stealing
  // First we must check if the block has been cached by another hart after we release the per bucket lock
  acquire(bcache.bucketlock + bucketno);
  for(b = bcache.hashtable[bucketno]; b != 0; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(bcache.bucketlock + bucketno);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  // Still not cached. We still cannot steal until we check if there is an unoccupied buffer in this bucket
  lru = hslru(bcache.hashtable[bucketno], 0);
  if(lru){
    lru->dev = dev;
    lru->blockno = blockno;
    lru->valid = 0;
    lru->refcnt = 1;
    release(bcache.bucketlock + bucketno);
    release(&bcache.lock);
    acquiresleep(&lru->lock);
    return lru;
  }
  // Now we can steal TODO: Is this a good buffer stealing scheme?
  for(int i = (bucketno + 1) % NBUCKET; i != bucketno; i = (i + 1) % NBUCKET){
    acquire(bcache.bucketlock + i); // But for the bcache global lock, this code was doomed to deadlock.
    struct buf *prev;
    lru = hslru(bcache.hashtable[i], &prev);
    if(lru){
      // move lru from bucket i to bucketno
      if(prev) // if lru is not the head of the linked list
        prev->next = lru->next;
      else // lru is the head of the linked list
        bcache.hashtable[i] = bcache.hashtable[i]->next;
      lru->next = bcache.hashtable[bucketno];
      bcache.hashtable[bucketno] = lru;
      // assign lru to the block
      lru->dev = dev;
      lru->blockno = blockno;
      lru->valid = 0;
      lru->refcnt = 1;
      release(bcache.bucketlock + i);
      release(bcache.bucketlock + bucketno);
      release(&bcache.lock);
      acquiresleep(&lru->lock);
      return lru;
    }
    release(bcache.bucketlock + i);
  }

  // Stage 4: Surrender
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

  uint bucketno = b->blockno % NBUCKET;
  acquire(bcache.bucketlock + bucketno);
  b->refcnt--;
  if (b->refcnt == 0) // no one is waiting for it.
    b->timestamp = ticks; 
  release(bcache.bucketlock + bucketno);
}

void
bpin(struct buf *b) {
  uint bucketno = b->blockno % NBUCKET;
  acquire(bcache.bucketlock + bucketno);
  b->refcnt++;
  release(bcache.bucketlock + bucketno);
}

void
bunpin(struct buf *b) {
  uint bucketno = b->blockno % NBUCKET;
  acquire(bcache.bucketlock + bucketno);
  b->refcnt--;
  release(bcache.bucketlock + bucketno);
}


