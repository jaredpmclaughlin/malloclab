/*
 ***********************************************************************************
 *                                   mm.c                                          *
 *  Starter package for a 64-bit struct-based explicit free list memory allocator  *        
 *                                                                                 *
 *  ********************************************************************************    
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <inttypes.h>

#include "memlib.h"
#include "mm.h"

 /* 
 *
 * Each block has header and footer of the form:
 * 
 *      63                  4  3  2  1  0 
 *      -----------------------------------
 *     | s  s  s  s  ... s  s  0  0  0  a/f
 *      ----------------------------------- 
 * 
 * where s are the meaningful size bits and a/f is set 
 * iff the block is allocated. The list has the following form:
 *
 *
 *    begin                                   end
 *    heap                                    heap  
 *  +-----------------------------------------------+
 *  | ftr(0:a)   | zero or more usr blks | hdr(0:a) |
 *  +-----------------------------------------------+
 *  |  prologue  |                       | epilogue |
 *  |  block     |                       | block    |
 *
 *
 * The allocated prologue and epilogue blocks are overhead that
 * eliminate edge conditions during coalescing.
 *
 */

/*  Empty block
 *  ------------------------------------------------*
 *  |HEADER:    block size   |     |     | alloc bit|
 *  |-----------------------------------------------|
 *  | pointer to prev free block in this size list  |
 *  |-----------------------------------------------|
 *  | pointer to next free block in this size list  |
 *  |-----------------------------------------------|
 *  |FOOTER:    block size   |     |     | alloc bit|
 *  ------------------------------------------------
 */

/*   Allocated block
 *   ------------------------------------------------*
 *   |HEADER:    block size   |     |     | alloc bit|
 *   |-----------------------------------------------|
 *   |               Data                            |
 *   |-----------------------------------------------|
 *   |               Data                            |
 *   |-----------------------------------------------|
 *   |FOOTER:    block size   |     |     | alloc bit|
 *   -------------------------------------------------
 */

/* Basic constants */

typedef uint64_t word_t;

// Word and header size (bytes)
static const size_t wsize = sizeof(word_t);

// Double word size (bytes)
static const size_t dsize = 2 * sizeof(word_t);

/*
  Minimum useable block size (bytes):
  two words for header & footer, two words for payload
*/
static const size_t min_block_size = 4 * sizeof(word_t);

/* Initial heap size (bytes), requires (chunksize % 16 == 0)
*/
//static const size_t chunksize = (1 << 12);    
static size_t chunksize = (1<<12);

// Mask to extract allocated bit from header
static const word_t alloc_mask = 0x1;

/*
 * Assume: All block sizes are a multiple of 16
 * and so can use lower 4 bits for flags
 */
static const word_t size_mask = ~(word_t) 0xF;

/*
  All blocks have both headers and footers

  Both the header and the footer consist of a single word containing the
  size and the allocation flag, where size is the total size of the block,
  including header, (possibly payload), unused space, and footer
*/

typedef struct block block_t;

/* Representation of the header and payload of one block in the heap */ 
struct block
{
    /* Header contains: 
    *  a. size
    *  b. allocation flag 
    */
    word_t header;

    union
    {
        struct
        {
            block_t *prev;
            block_t *next;
        } links;
        /*
        * We don't know what the size of the payload will be, so we will
        * declare it as a zero-length array.  This allows us to obtain a
        * pointer to the start of the payload.
        */
        unsigned char data[0];

    /*
     * Payload contains: 
     * a. only data if allocated
     * b. pointers to next/previous free blocks if unallocated
     */
    } payload;

    /*
     * We can't declare the footer as part of the struct, since its starting
     * position is unknown
     */
};

/* Global variables */

// Pointer to first block
static block_t *heap_start = NULL;

// Pointer to the first block in the free list
static block_t *free_list_head = NULL;

/* Function prototypes for internal helper routines */

//static size_t max(size_t x, size_t y);
static block_t *find_fit(size_t asize);
static block_t *coalesce_block(block_t *block);
static void split_block(block_t *block, size_t asize);

static size_t round_up(size_t size, size_t n);
static word_t pack(size_t size, bool alloc);

static size_t extract_size(word_t header);
static size_t get_size(block_t *block);

static bool extract_alloc(word_t header);
//static bool get_alloc(block_t *block);

static void write_header(block_t *block, size_t size, bool alloc);
static void write_footer(block_t *block, size_t size, bool alloc);

static block_t *payload_to_header(void *bp);
static void *header_to_payload(block_t *block);
static word_t *header_to_footer(block_t *block);

//static block_t *find_next(block_t *block);
static word_t *find_prev_footer(block_t *block);
//static block_t *find_prev(block_t *block);

//static bool check_heap();
//static void examine_heap();
static int in_heap(const void* p);

static block_t *extend_heap(size_t size);
static void insert_block(block_t *free_block);
static void remove_block(block_t *free_block);


/* 
 * mm_init - Initialize the memory manager 
 */
int mm_init(void)
{
    /* Create the initial empty heap */
    // initial heap is just big enough for header and footer
    word_t *start = (word_t *)(mem_sbrk(2*wsize));
    if ((ssize_t)start == -1) {
        printf("ERROR: mem_sbrk failed in mm_init, returning %p\n", start);
        return -1;
    }
    
    /* Prologue footer */
    start[0] = pack(0, true);
    /* Epilogue header */
    start[1] = pack(0, true); 

    /* Heap starts with first "block header", currently the epilogue header */
    heap_start = (block_t *) &(start[1]);

    /* Extend the empty heap with a free block of chunksize bytes */
    block_t *free_block = extend_heap(chunksize);
    if (free_block == NULL) {
        printf("ERROR: extend_heap failed in mm_init, returning");
        return -1;
    }
   
    // we're pretty much guaranteed to have a free block in the list
    insert_block(free_block);
    heap_start = free_block;

  return 0;
}

/* 
 * mm_malloc - Allocate a block with at least size bytes of payload 
 */
void *mm_malloc(size_t size)
{
    size_t asize;      // Allocated block size
    block_t *block;
    block_t *ret;
   
    if (size == 0) // Ignore spurious request
        return NULL;
    
    // Too small block
    if (size <= dsize) {        
        asize = min_block_size;
    } else {
        // Round up and adjust to meet alignment requirements    
        asize = round_up(size + dsize, dsize);
    }
    // search free list naively stop at first large enough

    block = find_fit(asize);
 
    if( block == NULL){
	 return NULL;
 	}
    if( asize > get_size(block)) printf("mm_malloc tried to split too small.\n"); 
    split_block(block,asize);

    ret = header_to_payload(block); 
    return ret;
}


/* 
 * mm_free - Free a block 
 */
void mm_free(void *bp)
{
    size_t asize = 0;
    block_t *free_block = NULL;

    if (bp == NULL) return;

    free_block = payload_to_header(bp);
    asize = get_size(free_block); 

    write_header(free_block, asize, false);
    write_footer(free_block, asize, false);

    free_block = coalesce_block(free_block); 
}

/*
 * insert_block - Insert block at the head of the free list (e.g., LIFO policy)
 */
static void insert_block(block_t *free_block)
{
   if(get_size(free_block) < min_block_size) return;

   if( free_block == free_list_head ) return;

   if( free_list_head != NULL ){ 
	// this is almost always going to be the case.
	free_list_head->payload.links.prev = free_block;
	free_block->payload.links.next = free_list_head;
	free_block->payload.links.prev = NULL;
	free_list_head = free_block;
    } else { 
	free_block->payload.links.next = NULL;
	free_block->payload.links.prev = NULL;
	free_list_head = free_block;
    }
}
/*
block_t *check_block(block_t *free_block){
	block_t *test = free_list_head;
	if(test == NULL) return NULL;
	while(test!=free_block){
		test = test->payload.links.next;
		if(test==NULL) return NULL;
	}
	return free_block;
}
*/

block_t *check_block(block_t *free_block){
	if (free_block == NULL) return NULL;

 	if ( in_heap(free_block)){
	   if ( free_block->payload.links.prev != NULL){
		if(!in_heap(free_block->payload.links.prev)) return NULL;
	   } 
	   if ( free_block->payload.links.next != NULL){
		if(!in_heap(free_block->payload.links.next)) return NULL;
	   }
	}
	return free_block;
}

/*
 * remove_block - Remove a free block from the free list
 */
static void remove_block(block_t *free_block) 
{
    block_t *prev = NULL;
    block_t *next = NULL;

    // nothing to do
    if(free_block == NULL) return;
    if(free_list_head == NULL) return; 

    prev = free_block->payload.links.prev;
    next = free_block->payload.links.next;

    // singleton 5.6%
    if(prev==NULL && next==NULL){
	 free_list_head = NULL;
	 return;
    }

    //head 49.5%
    if(prev==NULL && next!=NULL)
    {
        free_list_head = next;
        free_list_head->payload.links.prev = NULL;
	return;
    }

    //tail 2%
    if(prev!=NULL && next==NULL){
	 prev->payload.links.next=NULL;	
	 return; // if you don't, it can short circuit below
    }

    //middle 42.7%
    if(prev!=NULL && next!=NULL) 
    {
        prev->payload.links.next = next;
        next->payload.links.prev = prev;
    } 
    return; // just in case
}

/*
 * Finds a free block that of size at least asize
 */
static block_t *find_fit(size_t asize)
{
    block_t *cur = free_list_head;
    block_t *free_block;

    // the request is too small
    if(asize < min_block_size) return NULL;

    while(cur!=NULL){
	if(get_size(cur) >= (asize+dsize+min_block_size)) return cur;
	cur = cur->payload.links.next;
    }

    /* Didn't find anything, expand heap.
    *  For smaller requests, get a chunk, if it's bigger, just fulfill that
    *  maybe it's better to adjust the chunksize upwards if this is a repeated problem? 
    */

    asize = asize > chunksize ? (asize+dsize+min_block_size) : chunksize; 
    free_block = extend_heap(asize);

    // This is better for performance, the above is better for utilization
    //chunksize = asize > chunksize ? (asize+dsize+min_block_size) : chunksize;
    //free_block = extend_heap(chunksize);

    free_block = coalesce_block(free_block);

    return free_block;
}

/*
 * Coalesces current block with previous and next blocks if either or both are unallocated; otherwise the block is not modified.
 * Returns pointer to the coalesced block. After coalescing, the immediate contiguous previous and next blocks must be allocated.
 */
static block_t *coalesce_block(block_t *block)
{
    // assume block is not in free list 
    size_t blksz = get_size(block);
    word_t *prev_foot = find_prev_footer(block);
    word_t *next_head = &(block->header)+(long unsigned int)(blksz/wsize);
 
    bool next_alloc = extract_alloc(*next_head);
    bool prev_alloc = extract_alloc(*prev_foot); 
    size_t prev_sz = extract_size(*prev_foot);
    size_t next_sz = extract_size(*next_head);
    word_t *prev_head = prev_sz > 0 ? prev_foot - (prev_sz/wsize)+1 : prev_foot;
    word_t *next_foot = next_sz > 0 ? next_head + (next_sz/wsize)-1 : next_head; 
    // assume no change
    word_t *new_head = (word_t *)block;
    word_t *new_foot = (word_t *)block + (blksz/wsize)-1;;

    //don't attempt to coalesce singletons
    if(next_alloc && prev_alloc) return block;
    
    // find the header
    if ( !prev_alloc ) new_head = prev_head;
  
    // find the footer 
    if(!next_alloc) new_foot = next_foot;

    // calculate the total block size 
    if(!prev_alloc) blksz += prev_sz;
    if(!next_alloc) blksz += next_sz;

    // every time it's not in the list during a remove attempt, it's here. 
    // it's a small occurence, but if you try to remove, it will crash
    if(!prev_alloc){ 
	 if ( check_block((block_t *)(prev_head)) != NULL) { 
	 remove_block((block_t*)(prev_head));
	}
    }

    if(!next_alloc){
	 if ( check_block((block_t *)(next_head)) != NULL) {
	 remove_block((block_t *)(next_head));
	}
    }

    *new_head = pack(blksz, false);
    *new_foot = pack(blksz, false); 
    insert_block((block_t *)(new_head));
    return (block_t *)(new_head); 
}

/*
 * See if new block can be split one to satisfy allocation
 * and one to keep free
 */
static void split_block(block_t *block, size_t asize)
{
    size_t bsize = get_size(block);
    block_t *fblock = (block_t *)(((word_t *)(block))+(asize/wsize)) ;

    // avoid the chance of splitting blocks too small
    if( !(asize >= min_block_size) && !((bsize-asize) > min_block_size)) return;

    remove_block(block);	
     // put new block lower
    write_header(fblock, (bsize-asize), false); 
    write_footer(fblock, (bsize-asize), false);

    //fix up the /old/ block
    write_header(block, asize, true);
    write_footer(block, asize, true);

    insert_block(fblock);
}
/*
 * Extends the heap with the requested number of bytes, and recreates end header. 
 * Returns a pointer to the result of coalescing the newly-created block with previous free block, 
 * if applicable, or NULL in failure.
 */
static block_t *extend_heap(size_t size) 
{
    void *bp;

    // Allocate an even number of words to maintain alignment
    size = round_up(size, dsize);
    if ((bp = mem_sbrk(size)) == (void *)-1) {
        return NULL;
    }
    // bp is a pointer to the new memory block requested
    ((word_t *)(bp))[size/wsize] = pack(0,true); // new epilogue
    ((word_t *)(bp))[(size/wsize)-1] = pack(0,true); // new epilogue
    // over write old epilogue with header for new block
    ((word_t *)(bp))[-1] = pack(size,false);  
    //and set the footer to the same
    ((word_t *)(bp))[(size/wsize)-2] = pack(size,false);

    return payload_to_header(bp);
}

/******** The remaining content below are helper and debug routines ********/

/*
 * Return whether the pointer is in the heap.
 * May be useful for debugging.
 */
static int in_heap(const void* p)
{
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}
/*
 * examine_heap -- Print the heap by iterating through it as an implicit free list. 
 */
/*
static void examine_heap() {
  block_t *block;

  fprintf(stderr, "free_list_head: %p\n", (void *)free_list_head);

  for (block = heap_start; 
      get_size(block) > 0 && block < (block_t*)mem_heap_hi();
      block = find_next(block)) {

    fprintf(stderr, "%p: %ld %d\t", (void *)block, get_size(block), get_alloc(block));

    if (get_alloc(block)) {
      fprintf(stderr, "ALLOCATED\n");
    } else {
      fprintf(stderr, "FREE\tnext: %p, prev: %p\n",
      (void *)block->payload.links.next,
      (void *)block->payload.links.prev);
    }
  }
  fprintf(stderr, "END OF HEAP\n\n");
}
*/

/* check_heap: checks the heap for correctness; returns true if
 *               the heap is correct, and false otherwise.
 */
/*
static bool check_heap()
{

    // Implement a heap consistency checker as needed.


    if (!heap_start) {
        printf("NULL heap list pointer!\n");
        return false;
    }

    block_t *curr = heap_start;
    block_t *next;
    block_t *hi = mem_heap_hi();

    while ((next = find_next(curr)) + 1 < hi) {
        word_t hdr = curr->header;
        word_t ftr = *find_prev_footer(next);

        if (hdr != ftr) {
            printf(
                    "Header (0x%016lX) != footer (0x%016lX)\n",
                    hdr, ftr
                  );
            return false;
        }

        curr = next;
    }

    return true;
}
*/

/*
 *****************************************************************************
 * The functions below are short wrapper functions to perform                *
 * bit manipulation, pointer arithmetic, and other helper operations.        *
 *****************************************************************************
 */

/*
 * max: returns x if x > y, and y otherwise.
 */
/*
static size_t max(size_t x, size_t y)
{
    return (x > y) ? x : y;
}
*/

/*
 * round_up: Rounds size up to next multiple of n
 */
static size_t round_up(size_t size, size_t n)
{
    return n * ((size + (n-1)) / n);
}


/*
 * pack: returns a header reflecting a specified size and its alloc status.
 *       If the block is allocated, the lowest bit is set to 1, and 0 otherwise.
 */
static word_t pack(size_t size, bool alloc)
{
    return alloc ? (size | alloc_mask) : size;
}


/*
 * extract_size: returns the size of a given header value based on the header
 *               specification above.
 */
static size_t extract_size(word_t word)
{
    return (word & size_mask);
}


/*
 * get_size: returns the size of a given block by clearing the lowest 4 bits
 *           (as the heap is 16-byte aligned).
 */
static size_t get_size(block_t *block)
{
    return extract_size(block->header);
}


/*
 * extract_alloc: returns the allocation status of a given header value based
 *                on the header specification above.
 */
static bool extract_alloc(word_t word)
{
    return (bool) (word & alloc_mask);
}


/*
 * get_alloc: returns true when the block is allocated based on the
 *            block header's lowest bit, and false otherwise.
 */
/*
static bool get_alloc(block_t *block)
{
    return extract_alloc(block->header);
}
*/

/*
 * write_header: given a block and its size and allocation status,
 *               writes an appropriate value to the block header.
 */
static void write_header(block_t *block, size_t size, bool alloc)
{
    block->header = pack(size, alloc);
}


/*
 * write_footer: given a block and its size and allocation status,
 *               writes an appropriate value to the block footer by first
 *               computing the position of the footer.
 */
static void write_footer(block_t *block, size_t size, bool alloc)
{
    word_t *footerp = header_to_footer(block);
    *footerp = pack(size, alloc);
}


/*
 * find_next: returns the next consecutive block on the heap by adding the
 *            size of the block.
 */
/*
static block_t *find_next(block_t *block)
{
    return (block_t *) ((unsigned char *) block + get_size(block));
}
*/

/*
 * find_prev_footer: returns the footer of the previous block.
 */
static word_t *find_prev_footer(block_t *block)
{
    // Compute previous footer position as one word before the header
    return &(block->header) - 1;
}


/*
 * find_prev: returns the previous block position by checking the previous
 *            block's footer and calculating the start of the previous block
 *            based on its size.
 */
/*
static block_t *find_prev(block_t *block)
{
    word_t *footerp = find_prev_footer(block);
    size_t size = extract_size(*footerp);
    return (block_t *) ((unsigned char *) block - size);
}
*/

/*
 * payload_to_header: given a payload pointer, returns a pointer to the
 *                    corresponding block.
 */
static block_t *payload_to_header(void *bp)
{
    return (block_t *) ((unsigned char *) bp - offsetof(block_t, payload));
}


/*
 * header_to_payload: given a block pointer, returns a pointer to the
 *                    corresponding payload data.
 */
static void *header_to_payload(block_t *block)
{
    return (void *) (block->payload.data);
}


/*
 * header_to_footer: given a block pointer, returns a pointer to the
 *                   corresponding footer.
 */
static word_t *header_to_footer(block_t *block)
{
    return (word_t *) (block->payload.data + get_size(block) - dsize);
}
