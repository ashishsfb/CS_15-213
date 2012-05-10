/*
 * mm-.c
 *
 * Vidur Murali
 * vmurali@unix.andrew.cmu.edu
 * 
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */

/* My Macros */

/* Variables */
/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

#define WSIZE	4	/* Size of a word, header/footer (bytes) */
#define DSIZE	8	/* Size of a doubleword (bytes) */
#define CHUNK_SIZE 256	/* Amount of memory to extend heap by */

/* Functions */
#define MAX(x, y) ((x)>(y)?(x):(y))

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
#define SIZE_PTR(p)	((size_t*)(((char*)(p)) - SIZE_T_SIZE))

/* Get size and allocated fields from address p */
#define GET_SIZE(p)	(GET(p) & ~0x7)
#define GET_ALLOC(p)	(GET(p) & 0x1)

/* Given block pointer, compute address of header and footer */
#define HDPTR(blockPtr)	((char *)(blockPtr) - WSIZE)
#define FTPTR(blockPtr)	((char *)(blockPtr) + GET_SIZE(HDPTR(blockPtr)) - 2*WSIZE)

/* Gets the pointer for the next block */
#define NEXT_BLKP(blockPtr)	((char *)(blockPtr) + GET_SIZE(HDPTR(blockPtr)))
#define PREV_BLKP(blockPtr)	((char *)(blockPtr) - GET_SIZE(HDPTR(blockPtr)))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ( (size)|(alloc) )
/* Read and write a word at address p */
#define GET(p)		(*(unsigned int *)(p))
#define PUT(p, val)	(*(unsigned int *)(p) = (val))

/* My function headers */
static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void place(void *blockPtr, size_t asize);

/* Global Variables */
static void *heap_ptr = 0;

/*
 * Initialize: return -1 on error, 0 on success.
 *
 * mm_init - Called when a new trace starts
 *
 * @return 	-  (0) if all ok
 * 		- (-1) otherwise
 */
int mm_init(void)
{
	heap_ptr = mem_sbrk(4*WSIZE);

	PUT(heap_ptr, 0);	/* Alignment padding OR Heap Prologue */
	PUT( (heap_ptr + (1*WSIZE)), PACK(DSIZE, 1) );	/* Prologue header */
	PUT( (heap_ptr + (2*WSIZE)), PACK(DSIZE, 1) );	/* Prologue footer */
	PUT( (heap_ptr + (3*WSIZE)), PACK(0, 1) );	/* Epilogue header */
	
	heap_ptr += (2*WSIZE);

	if( (long)heap_ptr < 0 )
		return -1;

 	/* Extend the heap with a free block of CHUNK_SIZE bytes */
	if( extend_heap(CHUNK_SIZE) == NULL )
		return -1;

	return 0;
}



/*
 * malloc - Allocate a block by incrementing the brk pointer.
 * 	    Always allocate a block whose size is a 
 * 	     multiple of the alignment
 *
 * @return 	- generic pointer to first byte of allocated memory
 * 		- NULL if error occured during mem allocation
 */
void *malloc(size_t sizeToAlloc)
{
	/*int newsize = ALIGN(sizeToAlloc + SIZE_T_SIZE);
	unsigned char *p = mem_sbrk(newsize);
	
	if( (long)p < 0 )   if p is -1 or NULL, allocation error occured 
		return NULL;
	else
	{
		p += SIZE_T_SIZE;
		*SIZE_PTR(p) = newsize;
		
		return p;
	}*/
	
	size_t asize;	   /* Adjusted block size */
	size_t extendsize; /* Amount to extend heap by, if you run out of space */
	char *blockPtr;
	
	if( heap_ptr == 0 )
		mm_init();
	
	if( sizeToAlloc == 0 )
		return NULL;
		
	if( sizeToAlloc <= DSIZE )
		asize = 2*DSIZE;
	else
		asize = DSIZE * ( (sizeToAlloc + DSIZE + (DSIZE - 1)) / DSIZE );
	
	blockPtr = find_fit(asize);
	
	// Search the free list for a fit
	if( blockPtr != NULL )
	{
		place( blockPtr, asize );
		return blockPtr;
	}
	
	// No fit found. Get more memory and place the block
	extendsize = MAX(asize, CHUNK_SIZE);
	blockPtr = extend_heap(extendsize/WSIZE);
	if( blockPtr == NULL )
		return NULL;
	place( blockPtr, asize );
	
	return blockPtr;
}

/*
 * free - No idea how to do this yet //TODO
 *
 * @return 	- void
 */
void free (void *ptr)
{
	ptr = ptr;
}

/*
 * realloc - Allocate a new block of memory of 'sizeToAlloc' bytes
 * 	     Copies all the stuff from oldPtr's mem to newPtr's mem
 *
 * @return 	- generic pointer to first byte of newly allocated memory
 * 		- NULL if error occured during mem allocation
 */
void *realloc(void *oldPtr, size_t sizeToAlloc)
{
	if(oldPtr == NULL) /* if oldPtr didn't have any mem, no copying req */
		return malloc(sizeToAlloc);
		
	if( sizeToAlloc == 0 )
		return NULL;
		
	void *newPtr = malloc(sizeToAlloc);
	memcpy(newPtr,oldPtr,sizeToAlloc);
	return newPtr;
}

/*
 * calloc - Allocate the block and set it to zero.
 * 
 * @return 	- generic pointer to first byte of newly allocated memory
 * 		- NULL if error occured during mem allocation
 */
void *calloc (size_t nmemb, size_t sizeToAlloc)
{
/*	void *ptr = malloc(sizeToAlloc);
 *
 *	void *iter = ptr;
 *	for(int i=0; i<size; i++)
 *	{
 *		iter = 0;
 *		iter++;
 *	}
 *	return NULL;
*/
	nmemb = nmemb;
	sizeToAlloc = sizeToAlloc;
	return NULL;
}

/*
 *	Helper Functions
 */

/*
 * extend_heap - Extends heap with free block and returns its block pointer
 * 
 * @return 	- block pointer
 * 		- NULL if error occured during mem allocation
 */
static void *extend_heap(size_t words)
{
	char *blockPtr;
	size_t size;

	/* Allocate an even number of words to maintain alignment */
	size = (words % 2) ? ( (words+1)*WSIZE ):( words*WSIZE );
	
	blockPtr = mem_sbrk(size);
	if( (long)blockPtr<0 )
		return NULL;

	PUT( HDPTR(blockPtr), PACK(size, 0) );	/* Free block header */
	PUT( FTPTR(blockPtr), PACK(size, 0) );	/* Free block footer */
	PUT( HDPTR(NEXT_BLKP(blockPtr)), PACK(0, 1)); /* New epilogue header*/

	return blockPtr;
}

/*
 * find_fit	- Find a fit for a block with 'asize' bytes
 * 
 * @return 	- block pointer
 * 		- NULL if error occured during mem allocation
 */
static void *find_fit(size_t asize)
{
	void *bp; /* Block iterating pointer */

	for( bp = heap_ptr; GET_SIZE(HDPTR(bp)) > 0; bp = NEXT_BLKP(bp) )
	{
		if( !GET_ALLOC(HDPTR(bp)) && ( asize <= GET_SIZE(HDPTR(bp)) ) )
			return bp;
	}

	/* No fit */
	return NULL;
}

/*
 * place	- Place block of 'asize' bytes at start of free block bp and
 * 		  split if remainder would be at least minimum block size
 * 
 * @return 	- void
 */
static void place(void *bp, size_t asize)
{
	size_t csize = GET_SIZE(HDPTR(bp));

	if( (csize-asize) >= (2*DSIZE) )
	{
		PUT(HDPTR(bp), PACK(asize,1));
		PUT(FTPTR(bp), PACK(asize,1));

		bp = NEXT_BLKP(bp);

		PUT(HDPTR(bp), PACK(csize-asize,0));
		PUT(FTPTR(bp), PACK(csize-asize,0));
	}
	else
	{
		PUT(HDPTR(bp), PACK(csize,1));
		PUT(FTPTR(bp), PACK(csize,1));
	}
}

/*
 * Return whether the pointer is in the heap.
 * May be useful for debugging.
 *
static int in_heap(const void *p)
{
	return p <= mem_heap_hi() && p >= mem_heap_lo();
}*/

/*
 * Return whether the pointer is aligned.
 * May be useful for debugging.
 *
static int aligned(const void *p)
{
	return (size_t)ALIGN(p) == (size_t)p;
}*/

/*
 * mm_checkheap - There are no bugs in my code, so I don't need to check heap
 */
void mm_checkheap(int verbose)
{
	verbose = verbose;
}
