/*
 * mm-freelist.c
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
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
//#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif

/* Basic constants and macros */
//#define WSIZE 4 /* word size (bytes) */
#define DSIZE 8 /* doubleword size (bytes) */
#define CHUNKSIZE (1<<12) /* initial heap size (bytes) */
#define OVERHEAD 16 /* overhead of header and footer (bytes) */
 
#define MAX(x, y) ((x) > (y)? (x) : (y))
 
/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))
 
/* Read and write a word at address p */
#define GET(p) (*(size_t *)(p))
#define PUT(p, val) (*(size_t *)(p) = (val))

#define PUT_ADD(p,val) ((size_t *)(p) = (val))
 
/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRBP(bp) ((void *)(bp) - DSIZE)
#define FTRBP(bp) ((void *)(bp) + GET_SIZE(HDRBP(bp)) - 2 * DSIZE)

 
/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((void *)(bp) + GET_SIZE(((void *)(bp) - DSIZE)))
#define PREV_BLKP(bp) ((void *)(bp) - GET_SIZE(((void *)(bp) - 2 * DSIZE)))

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)

/* RRK Defined */

#define PREVP(p) ((void *)(p) + DSIZE)
#define NEXTP(p) ((void *)(p) + 2 * DSIZE)

//#define PREVBP(bp) ((void *)(bp))
#define NEXTBP(bp) ((void *)(bp) + DSIZE)

//#define HDRP(p) ((void *)(p))
#define FTRP(p) ((void *)(p) + GET_SIZE(p) - DSIZE)

static void **free_start;
static void *heap_listp;
static void *list_head;
static void *list_tail;

void block_info(void *bp);

static inline void remove_block(void* bp){
	
	char *prev, *next;
	
	dbg_printf("Removing following block:\n");
	block_info(bp);
	
	prev = (void*)GET(bp);
	next = (void*)GET(NEXTBP(bp));
	
	if(prev!=0) PUT(NEXTP((void*)prev),(void*)next);
			
	else {
		dbg_printf("no prev free block\n");
		list_head = next; 
		dbg_printf("new list_head = %p\n",list_head);
			
	}
		
	if(next!=0) PUT(PREVP(next),prev);
	else {
		dbg_printf("no next free block\n");
		list_tail = prev;
		
	}
	
	return;
}

static inline void add_block(void* ptr){
	
	char *tmp,*next;
	dbg_printf("Adding following block:\n");
	block_info(ptr);
	
	next = NEXTBP(ptr);
	tmp = HDRBP(ptr);
	
	PUT(ptr,0);
	PUT(next,(char*)list_head);
	
	if(list_head != 0){
		PUT(PREVP(list_head),tmp);
	}
	
	dbg_printf("next block in free list = %p\n",GET(next));
	list_head = tmp;

	if(list_tail==0) list_tail=list_head;
}



static inline void *coal_forward(void *bp){
	
	char *nbn,*nbp,*tmp,*ptr,*nblk;
	dbg_printf("case 2\n");

	ptr = HDRBP(bp);
	nblk = HDRBP(NEXT_BLKP(bp));
	
	size_t size = GET_SIZE(ptr)+GET_SIZE(nblk);  
	
	block_info((char *)nblk+DSIZE);
	dbg_printf("incremented size to %d\n",(int)size);
	tmp = (char *)nblk;
	
	size_t new = PACK(size, 0);
	
	PUT(HDRBP(bp), new); 
	PUT(FTRBP(bp), new); 
	
	if((char *)list_tail == tmp){
		dbg_printf("list_tail changing from p = %p to nblk = %p\n",bp-DSIZE,nblk);
		list_tail = (void *)GET(PREVP(nblk));
		if(list_tail==0) list_tail = ptr;
	}
	
	nbn = (char *)GET(NEXTP(nblk));
	nbp = (char *)GET(PREVP(nblk));
	
	if(nbn != 0) PUT(PREVP(nbn),nbp);
	if(nbp != 0) PUT(NEXTP(nbp),nbn);
	
	/*
	remove(PREVP(nblk));
	if(list_tail==0)list_tail = ptr;
	*/
	
	return(bp);
		
}

static inline void *coal_backward(void *bp,size_t size, void *pblk){
	
	dbg_printf("case 3\n");
	void *nbn,*nbp,*ptr;
	
	ptr = HDRBP(bp);
	
	block_info((char *)pblk+DSIZE);
	size += GET_SIZE(pblk);
	dbg_printf("incremented size to %d\n",(int)size);
	size_t new = PACK(size, 0);
	
	PUT(FTRBP(bp), new); 
	PUT(pblk, new); 
	
	
	dbg_printf("removing present block\n");
	
	
	if((char *)list_tail == ptr) list_tail = pblk;
	
	
	dbg_printf("list_tail changing from p = %p to nblk = %p\n",ptr,pblk);
	list_head = (void *)GET(NEXTP(ptr));
	if(list_head==0) list_head = pblk;
	
	nbn = (char *)GET(NEXTP(ptr));
	nbp = (char *)GET(PREVP(ptr));
	
	if(nbn != 0) PUT(PREVP(nbn),nbp);
	if(nbp != 0) PUT(NEXTP(nbp),nbn);
	block_info(pblk+DSIZE);
	return(pblk+DSIZE);
	
}

static inline void *coalesce(void *bp) 
{ 
	char *pblk, *nblk,*ptr,*tmp;
	
	block_info(bp);
	pblk = HDRBP(PREV_BLKP(bp));
	nblk = HDRBP(NEXT_BLKP(bp));
	ptr = HDRBP(bp);
	
	size_t prev_alloc = GET_ALLOC(pblk); 
	size_t next_alloc = GET_ALLOC(nblk); 
	size_t size = GET_SIZE(ptr); 
	
	if (prev_alloc && next_alloc) { /* Case 1 */ 
		dbg_printf("case 1\n");
		return bp; 
	} 

	else if (prev_alloc && !next_alloc) { /* Case 2 */ 
		
		dbg_printf("case 2\n");
		return coal_forward(bp); 
		//return bp;
	} 

	else if (!prev_alloc && next_alloc) { /* Case 3 */ 
		
		dbg_printf("case 3\n");
		return coal_backward(bp,size,pblk);
		
		//return coal_forward(PREVP(pblk));
		
		//return bp;
		
 	} 
 
	else { /* Case 4 */ 
		
		dbg_printf("case 4\n");
		
		tmp = coal_forward(bp);
		
		size = GET_SIZE(HDRBP(bp));
		
		return coal_backward(bp,size,pblk);
		
		//return tmp;
	} 
	
}

static void *extend_heap(size_t words)
{
    char *bp,*tmp;
    size_t size;
	size_t new;
    /* Allocate an even number of words to maintain alignment */
    size = words * DSIZE;
	new = PACK(size, 0);
    if ((int)(bp = mem_sbrk(size)) < 0){
     	dbg_printf("Extend Heap failed");
   		return NULL;
	}
	dbg_printf("Extend Heap: %d\n", (int)size);
	
    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRBP(bp), new); /* free block header */
    PUT(FTRBP(bp), new); /* free block footer */
    PUT(HDRBP(NEXT_BLKP(bp)), PACK(0, 1)); /* new epilogue header */
    
	dbg_printf("Calling add_block");
	add_block(bp);
	
	dbg_printf("Extend Heap Success with bp = %p\n",bp);
	dbg_printf("Size of extended heap = %d\n",GET_SIZE(HDRBP(bp)));
	
	/* Coalesce if the previous block was free */
	dbg_printf("Calling coalesce\n");
	return coalesce(bp);
}

/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void) {
	
	dbg_printf("Init start\n");
	
    /* create the initial empty heap */
    if ((heap_listp = mem_sbrk(4*DSIZE)) == NULL)    
        return -1;

    PUT(heap_listp, 0); /* alignment padding */
    PUT(heap_listp+DSIZE, PACK(OVERHEAD, 1)); /* prologue header */
    PUT(heap_listp+2*DSIZE, PACK(OVERHEAD, 1)); /* prologue footer */
    PUT(heap_listp+3*DSIZE, PACK(0, 1)); /* epilogue header */
   
	list_head = 0;//heap_listp+3*DSIZE;
	list_tail = 0;//heap_listp+3*DSIZE;
    heap_listp += 2*DSIZE;
 
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if ((extend_heap(CHUNKSIZE/DSIZE)) == NULL)
        return -1;

	dbg_printf("Init Success\n");
	
    return 0;
}

void block_info(void *bp){
		
		char *prev,*next;
		dbg_printf("Block address = %p\n",bp-DSIZE);
		dbg_printf("Block Size = %d\n",(int)(GET_SIZE((char*)(bp)-DSIZE)));
		dbg_printf("Base address = %p\n",bp);

		if(GET_ALLOC((char*)bp-DSIZE)==0){
			prev = GET(bp);
			next = GET(NEXTBP(bp));

			dbg_printf("prev = %p, next = %p\n",prev, next);
		}
	
	
}

void place(void *bp, size_t asize){
	
		dbg_printf("place start with address = %p and size = %d\n",bp,(int)asize);
	
    	char *nblk, *prev, *next;
		size_t size = GET_SIZE(HDRBP(bp));
	
		dbg_printf("got block size = %d\n",(int)size);
	
		size_t bsize = asize;
		size_t split  = size - asize;
		size_t tmp;
		
		int flg = 0;

		if(split<32){
			bsize = size;
			flg = 1;
		}
	
		dbg_printf("Size being allocated = %d\n",(int)bsize);
	
		tmp = PACK(bsize, 1);
    	PUT(HDRBP(bp), tmp); /* allocated block header */
    	PUT(FTRBP(bp), tmp); /* allocated block footer */
   
		dbg_printf("Footer address= %p\n",FTRBP(bp));
		
		
		prev = GET(bp);
		next = GET(NEXTBP(bp));
	
		dbg_printf("prev = %p, next = %p\n",prev, next);
		
		//if splitting can be done
		if(flg==0){
		
			dbg_printf("Splitting the block\n");
			
			nblk = bp + bsize - DSIZE; 
			
    		//PUT(NEXT_BLKP(bp),nblk);

			dbg_printf("next block = %p , size = %d\n",nblk,(int)split);
			
			tmp = PACK(split,0);
			PUT(nblk,tmp);
    		PUT(FTRP(nblk),tmp);
			
			/*
			//dbg_printf("list_tail points to %p and bp to %p\n",list_tail,(bp-DSIZE));
			
			//dbg_printf("list_head = %p, list_tail = %p",list_head,list_tail);
			
			dbg_printf("prev free block address = %p\n",GET(bp));
			
    		PUT(PREVP(nblk),prev);

			dbg_printf("next free block address = %p\n",GET(NEXTBP(bp)));

    		PUT(NEXTP(nblk),next);
			
			//not sure if right
			if(prev!=0) {
				dbg_printf("changing prev\n");
				PUT(NEXTP(prev),(char*)nblk);
				dbg_printf("Address of prev block next pointer = %p\n",NEXTP(prev));
				
				dbg_printf("Done changing prev\n");
				
				//block_info(prev+DSIZE);
			}
			
    		if(next!=0) {
				dbg_printf("changing next\n");			
				PUT(PREVP(next),nblk);
				dbg_printf("Done changing next\n");
			}
			
			
			if((char *)list_tail == ((char *)bp-DSIZE)){

				dbg_printf("list_tail changing from p = %p to nblk = %p\n",bp-DSIZE,nblk);
				list_tail = nblk;

			} 
			dbg_printf("between2\n");
			if((char *)list_head == ((char *)bp-DSIZE)){
				dbg_printf("list_head changing from p = %p to nblk = %p\n",bp-DSIZE,nblk);		
				list_head = nblk;
			}
			*/
			
			remove_block(bp);
			add_block(PREVP(nblk));
			dbg_printf("Splitting complete\n");
		
	}
	
	//whole block allocated
	else{
			
			dbg_printf("whole block allocated\n");
			remove_block(bp);
	}
    	
    	return;
}


void *find_fit(size_t asize){
	
	dbg_printf("find_fit start\n");
	
	void *tmp = list_head;
	dbg_printf("list head = %p\n",list_head);
	dbg_printf("list tail = %p\n",list_tail);
	
	size_t bsize;

	while(tmp !=0){
		dbg_printf("checking block %p\n",tmp);
		if((bsize = GET_SIZE(tmp))>= asize){
			
			dbg_printf("find_fit Success with bsize = %d\n",(int)bsize);
			dbg_printf("find_fit returning address %p\n",tmp+DSIZE);
			
			return tmp+DSIZE;
		}
		
		if((char *)tmp == (char *)list_tail) return NULL;
		dbg_printf("check next free block\n");
		tmp = GET(NEXTP(tmp));
	}
	
	dbg_printf("Find_fit failed\n");
	return NULL;
}	

/*
 * malloc
 */
void *malloc (size_t size) {

	size_t asize;
	size_t extendsize;
	char *bp;

	dbg_printf("malloc start with size = %d\n",(int)size);
	
	if(size<=0) return NULL;
	
	if(size<= 2*DSIZE) asize = 2 * OVERHEAD;
	else asize = ALIGN(size+OVERHEAD);

	if((bp=find_fit(asize)) != NULL){
		place(bp,asize);
		return bp;
	}

	extendsize = MAX(asize,CHUNKSIZE);
	if((bp=extend_heap(extendsize/DSIZE)) == NULL)
		return NULL;

	place(bp,asize);
	dbg_printf("malloc Success\n");
	
	return bp;

}

/*
 * free
 */
void free (void *ptr) {
	
	char *tmp,*next;
	
	dbg_printf("free start with ptr = %p\n",ptr);
    if(!ptr) return;
	
	tmp = HDRBP(ptr);
	next = NEXTBP(ptr);
	
	size_t size = GET_SIZE(tmp);
	dbg_printf("block to be freed has size = %d\n",(int)size);
	
	dbg_printf("list head = %p\n",(char *)list_head);
	dbg_printf("list tail = %p\n",list_tail);

	PUT(tmp,PACK(size,0));
	PUT(FTRBP(ptr),PACK(size,0));

	add_block(ptr);
	
	dbg_printf("calling coalesce\n");
	coalesce(ptr);
	dbg_printf("free successful\n");
	
}

/*
 * realloc - you may want to look at mm-naive.c
 */
void *realloc(void *oldptr, size_t size) {
	
	dbg_printf("realloc start with oldptr = %p, size = %d\n",oldptr,(int)size);
	  size_t oldsize;
	  void *newptr;

	  /* If size == 0 then this is just free, and we return NULL. */
	  if(size == 0) {
		dbg_printf("size=0 so calling free\n");
	    free(oldptr);
	    return 0;
	  }

	  /* If oldptr is NULL, then this is just malloc. */
	  if(oldptr == NULL) {
		dbg_printf("oldptr=NULL so calling malloc\n");	
	    return malloc(size);
	  }

	  newptr = malloc(size);

	  /* If realloc() fails the original block is left untouched  */
	  if(!newptr) {
	    return 0;
	  }

	  /* Copy the old data. */
	  oldsize = GET_SIZE((char*)oldptr-DSIZE);
	  if(size < oldsize) oldsize = size;
	  memcpy(newptr, oldptr, oldsize);

	  /* Free the old block. */
	  free(oldptr);

	  return newptr;

}

/*
 * calloc - you may want to look at mm-naive.c
 * This function is not tested by mdriver, but it is
 * needed to run the traces.
 */
void *calloc (size_t nmemb, size_t size) {
  size_t bytes = nmemb * size;
  void *newptr;

  newptr = malloc(bytes);
  memset(newptr, 0, bytes);
}	


/*
 * Return whether the pointer is in the heap.
 * May be useful for debugging.
 */
static int in_heap(const void *p) {
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Return whether the pointer is aligned.
 * May be useful for debugging.
 */
static int aligned(const void *p) {
    return (size_t)ALIGN(p) == (size_t)p;
}

/*
 * mm_checkheap
 */
void mm_checkheap(int verbose) {
	return;
}
