/*
 * mm.c
 * Name: Mimi Chen
 * Andrew ID: mimic1
 * Comments:
 * In this malloc lab, I extends the functions which offered in the
 * baseline.I defined a struct for the block. There are header and
 * a union in it, in the union, I put a struct for the pointers to
 * the prev and next free block, which is useful when I trace the 
 * free list. 
 * I finished this lab in two steps. Firstly, I 
 * implements the explict method to create only one free list
 * for free blocks. Secondly, I implements the segarated list.
 * In it I use ten diffrent list to store the root for diffrent
 * free block size(32,64,128,256,512...). 
 * I creates three functions for segarated list.
 * First, Insert function: it is used when I need to insert a block
 * into the list. I have to check whether the list is null or not.
 * if null, this block becomes root in this free list, if not,I 
 * insert the block before the root.
 * Second, Delete function: it is used when I need to delete a block
 * in a free list, I need to check the size of the block and find 
 * the fit list, then I have to check whether the prev and the next
 * is the root or the tail node.
 * Third, findfitlist: it is used for find the fit list for the free
 * block.
 * In the coalsce function , I just put the insert and delete in the
 * way when there is new free-block or the older one should be merged
 * The more detailed introduction is in the Documentation as bellows.
 ******************************************************************************
 *                               mm.c                                *
 *           64-bit struct-based segregated free list memory allocator        *
 *                                                                            *
 *  ************************************************************************  *
 *                               DOCUMENTATION                                *
 *                                                                            *
 *  ** STRUCTURE. **                                                          *
 *                                                                            *
 *  Both allocated and free blocks share the same header structure.           *
 *  HEADER: 8-byte, aligned to 8th byte of an 16-byte aligned heap, where     *
 *          - The lowest order bit is 1 when the block is allocated, and      *
 *            0 otherwise.                                                    *
 *          - The whole 8-byte value with the least significant bit set to 0  *
 *            represents the size of the block as a size_t                    *
 *            The size of a block includes the header and footer.             *
 *  FOOTER: 8-byte, aligned to 0th byte of an 16-byte aligned heap. It        *
 *          contains the exact copy of the block's header.                    *
 *  The minimum blocksize is 32 bytes.                                        *
 *                                                                            *
 *  Allocated blocks contain the following:                                   *
 *  HEADER, as defined above.                                                 *
 *  PAYLOAD: Memory allocated for program to store information.               *
 *  The size of an allocated block is exactly PAYLOAD + HEADER.               *
 *                                                                            *
 *  Free blocks contain the following:                                        *
 *  HEADER, as defined above.                                                 *
 *  FOOTER, as defined above.                                                 *
 *  Prev_block pointer, which will points to the previous block in the list   *
 *  Next_block pointer, which will points to the next block in the list       *
 *                                                                            *
 *  There is another kind of free block:                                      *
 *  HEADER, as defined above.                                                 *
 *  NEXt,  which will points to the next block in the list                    *                                              
 *                                                                            *
 *                                                                            *
 *                                                                            *
 *                                                                            *
 *                                                                            *
 *                                                                            *
 *                                                                            *
 *                                                                            *
 *                                                                            *
 *                                                                            *
 *                                                                            *
 *  The size of an unallocated block is at least 16 bytes.                    *
 *                                                                            *
 *  Block Visualization.                                                      *
 *                    block     block+8          block+size-8                 *
 *  Allocated blocks:   |  HEADER  |  ... PAYLOAD ...                         *
 *                                                                            *
 *                    block     block+8          block+size-8   block+size    *
 *  Unallocated blocks: |  HEADER  |  ... (empty) ...  |  FOOTER  |           *
*                    block     block+8          block+size                    *
 *  Unallocated 16-byte blocks: |  HEADER  |  ... (empty) ...  |              *
 *                                                                            *
 *  ************************************************************************  *
 *  ** INITIALIZATION. **                                                     *
 *                                                                            *
 *  The following visualization reflects the beginning of the heap.           *
 *      start            start+8           start+16                           *
 *  INIT: | PROLOGUE_FOOTER | EPILOGUE_HEADER |                               *
 *  PROLOGUE_FOOTER: 8-byte footer, as defined above, that simulates the      *
 *                    end of an allocated block. Also serves as padding.      *
 *  EPILOGUE_HEADER: 8-byte block indicating the end of the heap.             *
 *                   It simulates the beginning of an allocated block         *
 *                   The epilogue header is moved when the heap is extended.  *
 *                                                                            *
 *  ************************************************************************  *
 *  ** BLOCK ALLOCATION. **                                                   *
 *                                                                            *
 *  Upon memory request of size S, a block of size S + dsize, rounded up to   *
 *  16 bytes, is allocated on the heap, where dsize is 2*8 = 16.              *
 *  Selecting the block for allocation is performed by finding the first-n    *
 *  block that can fit the content based on a first-n-fit search              *
 *  policy.                                                                   *
 *  The search starts from the segragated list which match its size           *
 *  It sequentialyy goes through each block in the chosed free list, until    *
 *  either:                                                                   *
 *  --A sufficiently-large unallocated block is found                         *
 *  --The end of the segragated list array is reached                         *
 *  Otherwise--that is, when no                                               *
 *  sufficiently-large unallocated block is found--then more unallocated      *
 *  memory of size chunksize or requested size, whichever is larger, is       *
 *  requested through mem_sbrk, and the search is redone.                     *                                         
 *                                                                            *
 *                                                                            *
 ******************************************************************************
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/*
 * If you want debugging output, uncomment the following.  Be sure not
 * to have debugging enabled in your final submission
 */
//#define DEBUG

#ifdef DEBUG
/* When debugging is enabled, the underlying functions get called */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#define dbg_requires(...) assert(__VA_ARGS__)
#define dbg_ensures(...) assert(__VA_ARGS__) 
#define dbg_checkheap(...) mm_checkheap(__VA_ARGS__)
#define dbg_printheap(...) print_heap(__VA_ARGS__)
#else
/* When debugging is disabled, no code gets generated */
#define dbg_printf(...)
#define dbg_assert(...)
#define dbg_requires(...)
#define dbg_ensures(...)
#define dbg_checkheap(...)
#define dbg_printheap(...)
#endif

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#define memset mem_memset
#define memcpy mem_memcpy
#endif /* def DRIVER */

/* What is the correct alignment? */
#define ALIGNMENT 16

/* Basic constants */
typedef uint64_t word_t;
static const size_t wsize = sizeof(word_t);   // word and header size (bytes)
static const size_t dsize = 2*wsize;          // double word size (bytes)
//static const size_t min_block_size = 2*dsize; // Minimum block size
static const size_t chunksize = (1 << 11);    // requires (chunksize % 16 == 0)

static const word_t alloc_mask = 0x1;
static const word_t size_mask = ~(word_t)0xF;

/* rounds up to the nearest multiple of ALIGNMENT */
static size_t align(size_t x) {
    return ALIGNMENT * ((x-1)/ALIGNMENT);
}
/*
 * We set the union to differ the different struct 
 * for the free block and the alloc block.
 */
typedef struct block  block_t;
struct  block
{
    word_t header;
    union {
        char payload[0];
        struct {
            block_t *prev_block;
            block_t *next_block;
        };
    };
};
/*
 * Define a 16bytes block struct with only header and next pointer
 */
typedef struct blockf  block_f;
struct  blockf
{
    word_t header;
    block_f *next;
};
/* Global variables */
/* Pointer to first block */
static block_t *heap_listp = NULL;
static block_t *currs[10];
static block_f *root16;

/* Function prototypes for internal helper routines */
static block_t *extend_heap(size_t size);
static void place(block_t *block, size_t asize);
static block_t *find_fit(size_t asize);
static block_t *coalesce(block_t *block);

static size_t max(size_t x, size_t y);
static size_t round_up(size_t size, size_t n);
static word_t pack(size_t size, bool alloc);
static word_t pack_prev(size_t size,bool alloc, bool prev_alloc);

static size_t extract_size(word_t header);
static size_t get_size(block_t *block);
static size_t get_payload_size(block_t *block);

static bool extract_alloc(word_t header);
static bool get_alloc(block_t *block);
static bool get_prev_alloc(block_t *block);

static void write_header(block_t *block,size_t size, bool prev,bool alloc);
static void write_footer(block_t *block, size_t size, bool alloc);

static block_t *payload_to_header(void *bp);
static void *header_to_payload(block_t *block);

static block_t *find_next(block_t *block);
static word_t *find_prev_footer(block_t *block);
static block_t *find_prev(block_t *block);
static void insert(block_t *block);
static void delete(block_t *block);
static int findfitlist(size_t size);
static void set_16_bit(block_t *block, bool alloc16);
static void reset_16_bit(block_t *block);
static bool get_16_bit(block_t *block);
static void insert_sixteen(block_f *block);
static void delete_sixteen(block_f *block);
static void delete_choose(block_t *block);
static void insert_choose(block_t *block);
static bool aligned_16bytes(const void *p);


bool mm_checkheap(int lineno);
/*
 * insert:insert the block in the list 
 */
static void insert(block_t *block)
{
    int i = findfitlist(get_size(block));
    if (currs[i] == NULL) {
        block->next_block = NULL;
        block->prev_block = NULL;
        currs[i] = block;
    } else {
        block->next_block = currs[i];
        currs[i]->prev_block = block;
        block->prev_block = NULL;
        currs[i] = block;
    }   
    
}
/*
 * insert 16 bytes to the 16 bytes free list.
 */
static void insert_sixteen(block_f *block) 
{
    if (root16 == NULL) {
        block->next = NULL;
        root16 = block;
    } else {
        block->next = root16;
        root16 = block;
    }
}
/*
 * delete:delete the block in the list 
 */
static void delete(block_t *block)
{
    block_t *prev = block->prev_block;
    block_t *next = block->next_block; 
    int i = findfitlist(get_size(block));
    //Block is the root , and only root in the list
    if (prev == NULL && next == NULL) {
       currs[i] = NULL;

    } 
    //Block is the root.
    else if (prev == NULL && next !=NULL) {
        next->prev_block=NULL;
        currs[i] = next;
    }
    //Block is the last one.
    else if (prev != NULL && next ==NULL) {
        prev->next_block = NULL;
    }
    //Block is in the middle.
    else if (prev != NULL && next != NULL) {
        prev->next_block = next;
        next->prev_block = prev;
    } 
}
/*
 * delete 16 bytes to the 16 bytes free list.
 */
static void delete_sixteen(block_f *block)
{
    if (root16 == block) {
        root16 = root16->next;
        return;
    }
    block_f *cur = root16;
    while (cur != NULL) {
        block_f *prev = cur;
        cur = cur->next;
        if (cur == block) {
            prev->next = cur->next;
            return;
        }
    }
}
/*
 * Choose what kind of delete function should I choose.
 */
static void delete_choose(block_t *block) 
{
    if (get_size(block) == dsize) {
        delete_sixteen((block_f *)block);
    }else {
        delete(block);
    }
}
/*
 * Choose what kind of insert function should I choose.
 */
static void insert_choose(block_t *block)
{
    if (get_size(block) == dsize) {
        insert_sixteen((block_f *)block);
    }else {
        insert(block);
    }
}
/*
 * find fit list: return index of the segarated array.
 */
static int findfitlist(size_t size)
{   
    if (size <= 32) {  //case 1: size <=32bytes;
        return 0; 
    }
    if (size <= 64) {  //case 2: size <=64bytes;
        return 1;  
    }
    if (size <= 128) {  //case 3: size <=128bytes;
        return 2; 
    }
    if (size <= 256) {  //case 4: size <=256bytes;
        return 3; 
    }
    if (size <= 512) {  //case 5: size <=512bytes;
        return 4; 
    }
    if (size <= 2048) {  //case 6: size <=2048bytes;
        return 5; 
    }
    if (size <= 4096) {  //case 7: size <=4096bytes;
        return 6; 
    }
    if (size <= 9192) {  //case 8: size <=9192bytes;
        return 7; 
    }
    if (size <= 18384) {  //case 9: size <=18384bytes;
        return 8; 
    }
    else {               //case 10: size >= 18384bytes;
        return 9;
    }
    
}
/*
 * Initialize: return false on error, true on success.
 */
bool mm_init(void) {
    // Create the initial empty heap 
    word_t *start = (word_t *)(mem_sbrk(2*wsize));

    if (start == (void *)-1) 
    {
        return false;
    }
    start[1] = pack_prev(0, true, true); // Epilogue header
    start[0] = pack_prev(0, true, false); // Prologue header
    // Heap starts with first block header (epilogue)
    heap_listp = (block_t *) &(start[1]);
    //Initialize the segragated list
    for (int i = 0; i < 10; i++) {
        currs[i] = NULL;   
    }
    //Initialize the single linked list for 16bytes
    root16 = NULL;
    // Extend the empty heap with a free block of chunksize bytes
    if (extend_heap(chunksize) == NULL)
    {
        return false;
    }
    return true;
}

/*
 * malloc
 */
void *malloc (size_t size) {
    size_t asize;      // Adjusted block size
    size_t extendsize; // Amount to extend heap if no fit is found
    block_t *block;
    void *bp = NULL;

    if (heap_listp == NULL) // Initialize heap if it isn't initialized
    {
        mm_init();
    }

    if (size == 0) // Ignore spurious request
    {
        dbg_ensures(mm_checkheap);
        return bp;
    }
    // Adjust block size to include overhead and to meet alignment requirements
    asize = round_up(size + wsize, dsize);    
    // Search the free list for a fit
    block = find_fit(asize);
    
    // If no fit is found, request more memory, and then and place the block
    if (block == NULL)
    {  
        extendsize = max(asize, chunksize);
        block = extend_heap(extendsize);
        if (block == NULL) // extend_heap returns an error
        {
            return bp;
        }

    }
    place(block, asize);
    bp = header_to_payload(block);
    return bp;
}

/*
 * free
 */
void free (void *bp) {
    if (bp == NULL)
    {
        return;
    }
    block_t *block = payload_to_header(bp);
    size_t size = get_size(block);
    
    bool prev_alloc = get_prev_alloc(block);
    write_header(block, size, prev_alloc, false);
    //Only write footer when the size is larger than 16bytes
    if (size > dsize) {
        write_footer(block, size, false);
    }
    coalesce(block);
}

/*
 * realloc
 */
void *realloc(void *ptr, size_t size) {
    block_t *block = payload_to_header(ptr);
    size_t copysize;
    void *newptr;

    // If size == 0, then free block and return NULL
    if (size == 0)
    {
        free(ptr);
        return NULL;
    }

    // If ptr is NULL, then equivalent to malloc
    if (ptr == NULL)
    {
        return malloc(size);
    }

    // Otherwise, proceed with reallocation
    newptr = malloc(size);
    // If malloc fails, the original block is left untouched
    if (!newptr)
    {
        return NULL;
    }

    // Copy the old data
    copysize = get_payload_size(block); // gets size of old payload
    if(size < copysize)
    {
        copysize = size;
    }
    memcpy(newptr, ptr, copysize);

    // Free the old block
    free(ptr);

    return newptr;
}

/*
 * calloc
 * This function is not tested by mdriver
 */
void *calloc (size_t nmemb, size_t size) {
    dbg_requires(mm_checkheap);
    void *bp;
    size_t asize = nmemb * size;

    if (asize/nmemb != size)
    // Multiplication overflowed
    return NULL;
    
    bp = malloc(asize);
    if (bp == NULL)
    {
        return NULL;
    }
    // Initialize all bits to 0
    memset(bp, 0, asize);

    return bp;
}


/*
 * Return whether the pointer is in the heap.
 * May be useful for debugging.
 */
static bool in_heap(const void *p) {
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Return whether the pointer is aligned.
 * May be useful for debugging.
 */
static bool aligned(const void *p) 
{
    size_t ip = (size_t) p;
    return align(ip) == ip;
}

static bool aligned_16bytes(const void *p) 
{
    size_t ip = (size_t) p;
    return (ip % dsize) == 0;
}


static block_t *extend_heap(size_t size) 
{
    void *bp;
    // Allocate an even number of words to maintain alignment
    size = round_up(size, dsize);
    if ((bp = mem_sbrk(size)) == (void *)-1)
    {
        return NULL;
    }
    
    // Initialize free block header/footer 
    block_t *block = payload_to_header(bp);
    bool prev_alloc = get_prev_alloc(block);
    write_header(block, size, prev_alloc , false);
    write_footer(block, size, false);

    // Create new epilogue header
    block_t *block_next = find_next(block);
    write_header(block_next, 0, false, true);
    // Coalesce in case the previous block was free
    return coalesce(block);
}

/* Coalesce: Coalesces current block with previous and next blocks if either
 *           or both are unallocated; otherwise the block is not modified.
 *           Returns pointer to the coalesced block. After coalescing, the
 *           immediate contiguous previous and next blocks must be allocated.
 */
static block_t *coalesce(block_t * block) 
{

    /*there has to add a situation that the extend_heap called this function 
    * that we have to change the last one pointer to this.
    */
    block_t *block_next = find_next(block);
    block_t *block_prev;
    bool prev_alloc = get_prev_alloc(block);
    bool next_alloc = get_alloc(block_next);
    size_t size = get_size(block);
    // Case 1 the prev and the next block are all allocated
    if (prev_alloc && next_alloc)              
    {
        write_header(block, size, true, false);
        if (size > dsize) {
            write_footer(block, size, false);
            insert(block);
        } else {
            insert_sixteen((block_f *)block);
        }
        return block;
    }
    // Case 2 only the prev block is allocated.
    else if (prev_alloc && !next_alloc)        
    {
        delete_choose(block_next);
        size += get_size(block_next);      
        write_header(block, size, true, false);
        write_footer(block, size, false);
        insert(block);
        block_next = find_next(block);
        reset_16_bit(block_next);      
    }
    // Case 3 only the next block is allocated
    else if (!prev_alloc && next_alloc)        
    {
        //Check the size and decide which list should it delete from.
        if (get_16_bit(block)) {
            block_f *block_prev16 = (block_f *)((char *)block - dsize);
            delete_sixteen(block_prev16); 
            block_prev = (block_t *)block_prev16; 
        } else {
            block_prev = find_prev(block);
            delete(block_prev);
        }  
        size += get_size(block_prev);
        write_header(block_prev, size, true, false);
        write_footer(block_prev, size, false);
        insert(block_prev);
        block = block_prev;
        reset_16_bit(block_next);
    }
    // Case 4 neither of these two is allocated
    else                                        
    {
        //Check the size and decide which list should it delete from.
        if (get_16_bit(block)) {
            block_f *block_prev16 = (block_f *)((char *)block - dsize);
            delete_sixteen(block_prev16); 
            block_prev = (block_t *)block_prev16; 
        } else {
            block_prev = find_prev(block);
            delete(block_prev);
        }
        size += get_size(block_next) + get_size(block_prev);
        delete_choose(block_next);
        write_header(block_prev, size, true, false);
        write_footer(block_prev, size, false);
        insert(block_prev);
        block = block_prev;
        block_next = find_next(block);
        reset_16_bit(block_next);
    }
    return block;
}

/*
 * place: Places block with size of asize at the start of bp. If the remaining
 *        size is at least the minimum block size, then split the block to the
 *        the allocated block and the remaining block as free, which is then
 *        inserted into the segregated list. Requires that the block is
 *        initially unallocated.
 */
static void place(block_t *block, size_t asize)
{
    size_t csize = get_size(block);
    bool prev_alloc = get_prev_alloc(block);
    if ((csize - asize) >= dsize)
    {
        block_t *block_next;
        delete(block);
        write_header(block, asize, prev_alloc, true);
       // write_footer(block, asize, true);
        block_next = find_next(block);
        write_header(block_next, csize-asize, true, false);
        //decide whether is should write footer on its size
        if (csize - asize > dsize)
        {
            write_footer(block_next, csize-asize, false);
            insert(block_next);
        }
        else
        {
            insert_sixteen((block_f *)block_next);
        }
    }
    else
    { 
        delete_choose(block);
        write_header(block, csize, prev_alloc, true);
    }
}

/*
 * find_fit: Looks for a free block with at least asize bytes with
 *           first-fit policy. Returns NULL if none is found.
 */
static block_t *find_fit(size_t asize)
{
    //Check the size == 16bytes.
    if (asize == dsize && root16 != NULL) 
    {
        return (block_t *)root16;
    }
    //Check the size == 32bytes.
    if (asize == 2 * dsize && currs[0] != NULL) 
    {
        return currs[0];
    }
    int index = findfitlist(asize);
    int j = index;
    int n_max = 12;
    int n = n_max;
    block_t *blockfit = NULL;
    /*
    *Go through the whole segraged list and choose the best from
    *the first-n fit.
    */
    for (j=index; j < 10; j++) {
        block_t *root1 = currs[j];    
        block_t *block;
        for (block = root1; get_size(block) > 0;
                             block = block->next_block )
        {
            size_t size = get_size(block);
            if ( asize <= size)
            {
                size_t fit_size = get_size(blockfit);
                 
                if (blockfit == NULL || fit_size > size)
                {
                    blockfit = block;
                }
                n--;
                if (n == 0)
                {
                    return blockfit;
                }
            }
        }
        /*
        *If I could find in the smaller list just return it
        */
        if (n < n_max)
        {
            return blockfit;
        }
    }
    return NULL; // no fit found
}

/*
 * max: returns x if x > y, and y otherwise.
 */
static size_t max(size_t x, size_t y)
{
    return (x > y) ? x : y;
}
/*
 * round_up: Rounds size up to next multiple of n
 */
static size_t round_up(size_t size, size_t n)
{

    return (n * ((size + (n-1)) / n));
}
/*
 * pack: returns a header reflecting a specified size and its alloc status.
 * If the block is allocated, the lowest bit is set to 1, and 0 otherwise.
 */
static word_t pack(size_t size, bool alloc)
{
    return alloc ? (size | 1) : size;
}

/*
 * pack_prev: returns a header reflecting the prev block's alloc status.
 *       If the block is allocated, the second lowest bit is set to 1,
 *       and 0 otherwise.
 */
static word_t pack_prev(size_t size, bool alloc, bool prev_alloc)
{
    int s = 0;
    if (alloc == 1)
    {
        s = 1;
    } 
    return prev_alloc ? (size | 2 | s) : (size|s);
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
    if (block == NULL) return 0;
    return extract_size(block->header);
}
/*
 * get_payload_size: returns the payload size of a given block, equal to
 *                   the entire block size minus the header and footer sizes.
 */
static word_t get_payload_size(block_t *block)
{
    if (block == NULL) return 0;
    size_t asize = get_size(block);
    if (get_alloc(block))
    {
        return asize - wsize;
    }
    return asize - dsize;
}
/*
 * extract_alloc: returns the allocation status of a given header value based
 *                on the header specification above.
 */
static bool extract_alloc(word_t word)
{
    return (bool)(word & alloc_mask);
}
/*
 * get_alloc: returns true when the block is allocated based on the
 *            block header's lowest bit, and false otherwise.
 */
static bool get_alloc(block_t *block)
{
    return extract_alloc(block->header);
}
/*
 * get_prev_alloc: returns true when the previous
 *             block is allocated based on the block header's second 
 *             lowest bit, and false otherwise.
 */
static bool get_prev_alloc(block_t *block)
{
    word_t head = block->header;
    return (bool)(head & (alloc_mask << 1));
}
/*
 * write_header: given a block and its size and allocation status,
 *               writes an appropriate value to the block header.
 * also write the 16vytes flag to the next block.
 */
static void write_header(block_t *block,size_t size,bool prev_alloc,bool alloc)
{
    bool sign16 = get_16_bit(block);
    block->header = pack_prev(size, alloc, prev_alloc);
    set_16_bit(block, sign16);
    block_t *next_block = find_next(block);
    bool own_alloc = get_alloc(next_block);
    next_block->header = pack_prev(get_size(next_block),own_alloc, alloc);
    if (size == dsize) {
        set_16_bit(next_block, true);
    } else {
        reset_16_bit(next_block);
    }
}
/*
 * write_footer: given a block and its size and allocation status,
 *               writes an appropriate value to the block footer by first
 *               computing the position of the footer.
 */
static void write_footer(block_t *block, size_t size, bool alloc)
{
    word_t *footerp = (word_t *)((block->payload) + get_size(block) - dsize);
    *footerp = pack(size, alloc);
}


/*
 * find_next: returns the next consecutive block on the heap by adding the
 *            size of the block.
 */
static block_t *find_next(block_t *block)
{
    dbg_requires(block != NULL);
    block_t *block_next = (block_t *)(((char *)block) + get_size(block));

    dbg_ensures(block_next != NULL);
    return block_next;
}

/*
 * find_prev_footer: returns the footer of the previous block.
 */
static word_t *find_prev_footer(block_t *block)
{
    // Compute previous footer position as one word before the header
    return (&(block->header)) - 1;
}

/*
 * find_prev: returns the previous block position by checking the previous
 *            block's footer and calculating the start of the previous block
 *            based on its size.
 */
static block_t *find_prev(block_t *block)
{
    
    word_t *footerp = find_prev_footer(block);
    size_t size = extract_size(*footerp);
    return (block_t *)((char *)block - size);
}
/*
 * payload_to_header: given a payload pointer, returns a pointer to the
 *                    corresponding block.
 */
static block_t *payload_to_header(void *bp)
{
    if(bp == NULL) return NULL;
    return (block_t *)(((char *)bp) - offsetof(block_t, payload));
}
/*
 * header_to_payload: given a block pointer, returns a pointer to the
 *                    corresponding payload.
 */
static void *header_to_payload(block_t *block)
{
    if(block == NULL) return NULL;
    return (void *)(block->payload);
}
/*
 * set the 16byte flag for the prev block;
 */
static void set_16_bit(block_t *block, bool alloc16)
{
    if (alloc16) 
    {
        block->header = block->header | 0x4;
    } 
    else 
    {
        block->header = block->header & (word_t)(~0x4);
    }
}

/*
 * reset the 16byte flag for the prev block;
 */
static void reset_16_bit(block_t *block)
{
    block->header = block->header & (word_t)(~0x4);
}
/*
 * get the 16byte flag for the prev block;
 */
static bool get_16_bit(block_t *block)
{
    return (bool)(block->header & 0x4);
}

/*
 * mm_checkheap
 */
bool mm_checkheap(int lineno) {

    if (!heap_listp) {
        printf("NULL heap list pointer line:%d\n",lineno);
        return false;
    }
    
    block_t *next;
    block_t *curr = heap_listp;
    block_t *hi = mem_heap_hi();
    int a = 0;
    int b = 0;
    size_t size_list[10];
    int two = 2;
    for (int i = 0; i <10; i ++) 
    {
        size_list[i] = (two << i)*dsize;
    }
    block_f *root_16 = root16;
    while (root_16 != NULL) 
    {
        b++;
        if (get_size((block_t *)root_16) != dsize) 
        {
            printf("size not match the 16 bytes list line:%d\n",lineno);
            return false;
        }
        if (!in_heap((block_t *)root_16)) 
        {
            printf("free block is not int heap:%d\n",lineno);
            return false;
        }
        root_16 = root_16->next;

    }
    /* 
    * In this while loop we check all the block whether
    * the previous footer is equals to the curreent header
    */
    while ((next = find_next(curr)) + 1 < hi) 
    {
        if(!get_alloc(curr))
        {
            a++;
        }
        if (!in_heap(curr)) 
        {
            printf("free block is not int heap:%d\n",lineno);
            return false;
        }
        if (!get_alloc(curr) && !get_alloc(next)) 
        {
            printf("Two consecutive free blocks in the heap.");
            return false;
        }
        if (get_alloc(curr) != get_prev_alloc(next)) 
        {
            printf("Prev alloc not consistent.");
            return false;
        }
        if (get_size(curr) < dsize) 
        {
            printf("Small than min_block_size.");
            return false;
        }

        curr = next;
    }
    for (int i = 0; i < 10; i++) {
        block_t *node = currs[i];
        block_t *next2;
        while (node != NULL) {
            b++;
            next2 = node->next_block;
            if (i < 9 && get_size(node) > size_list[i]) 
            {
                printf("size not match the list line:%d\n",lineno);
                return false;
            }
            if (i == 9 && get_size(node) <= size_list[8]) 
            {
                printf("size not match the list line:%d\n",lineno);
                return false;
            }
            if (next2 != NULL) 
            {
                block_t *prev_node = next2->prev_block;
                if (prev_node != node) 
                {
                    printf("Prev != curr->prev line:%d\n",lineno);
                    return false;
                }
                if (!in_heap(next2)) 
                {
                    printf("free block is not int heap:%d\n",lineno);
                    return false;
                }
            }
            node = next2;
        }
    }
    return true;
}

