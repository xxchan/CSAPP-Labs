/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Tianxiao Shen",
    /* First member's email address */
    "xxchan@sjtu.edu.cn",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

#define CLASSNUM 12

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)

#define GET(p)      (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define PACK(size, alloc) ((size) | (alloc))
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))

#define PREDP(bp) ((char *)(bp))
#define SUCCP(bp) ((char *)(bp)+WSIZE)

#define PRED_BLKP(bp) (*(char **)PREDP(bp))
#define SUCC_BLKP(bp) (*(char **)SUCCP(bp))

#define PUTP(p, pval) (*(char **)(p) = (char *)(pval))
/* replace PUTP with UPDATE_SUCC, UPDATE_PRED */
#define UPDATE_SUCC(p, succ) \
{ \
    if(p > heap_listp + WSIZE * CLASSNUM) \
        PUTP(SUCCP(p), succ); \
    else \
        PUTP(p, succ); \
}  
#define UPDATE_PRED(p, pred) if(p) PUTP(PREDP(p), pred)

// #define CHECK(bp) (printf("bp:%u, SIZE:%d, ALLOC:%d\n",(unsigned int)bp, GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp))))
// #define DEBUG

#ifdef DEBUG
# define DBG_PRINTF(...) fprintf(stderr, __VA_ARGS__)
# define CHECKHEAP() mm_check()
#else
# define DBG_PRINTF(...)
# define CHECKHEAP()
#endif /* DEBUG */

static char *heap_listp = 0;

int get_class(size_t aligned_size);

int mm_check(void)
{
    printf("mm_check\n");
    void *bp;
    int cnt;
    for(int c=0; c<=CLASSNUM; ++c)
    {
        bp = *(char **)(heap_listp + c*WSIZE);
        printf("\tcheck class %d, %u\n",c,(unsigned int)(heap_listp+c*WSIZE));
        cnt = 0;
        while(bp)
        {
            printf("\tcheck node %u, SIZE %u, ALLOC %u, PRED %u, SUCC %u\n", 
                (unsigned int)bp, (unsigned int)GET_SIZE(HDRP(bp)),(unsigned int)GET_ALLOC(HDRP(bp)),(unsigned int)PRED_BLKP(bp),(unsigned int)SUCC_BLKP(bp));
            bp = SUCC_BLKP(bp);
            cnt++;
            if(cnt > 10)
            {
                printf("maybe dead loop"); return -1;
            }
        }
    }
    printf("end mm_check\n");
    // printf("check epilogue %u, SIZE %u, ALLOC %u, PRED %u\n", (unsigned int)bp,(unsigned int)GET_SIZE(HDRP(bp)),(unsigned int)GET_ALLOC(HDRP(bp)),(unsigned int)PRED_BLKP(bp)); 
    return 0;
}
void delete_node(void *bp);
inline void delete_node(void *bp)
{
    char *succ = SUCC_BLKP(bp), *pred = PRED_BLKP(bp);
    UPDATE_SUCC(pred, succ);
    UPDATE_PRED(succ, pred);
}

void insert_node(void *bp);
/* insert after prologue */
inline void insert_node(void *bp)
{
    int class = get_class(GET_SIZE(HDRP(bp)));
    char **class_head = heap_listp + class*WSIZE;
    char *old_fst = *class_head;
    UPDATE_PRED(bp, class_head);
    UPDATE_SUCC(bp, old_fst);
    UPDATE_PRED(old_fst,bp);
    PUTP(class_head, bp);
}

static void *coalesce(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    DBG_PRINTF("before coalesce %u, size %u\n",(unsigned int)bp, size); 
    DBG_PRINTF("prev blkp %u\n",PREV_BLKP(bp));
    DBG_PRINTF("heap+14 %u %u\n",(heap_listp+14*WSIZE),GET(heap_listp+14*WSIZE));
    DBG_PRINTF("1 %u 2 %u 3 %u \n",(char *)(bp),(char *)(bp) - DSIZE, GET_SIZE((char *)(bp) - DSIZE));
    CHECKHEAP();
    unsigned int prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    unsigned int next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    DBG_PRINTF("prev %u %u next %u %u\n",PREV_BLKP(bp),prev_alloc,NEXT_BLKP(bp),next_alloc);
    if (prev_alloc && next_alloc)
    {
        return bp;
    }

    else if (prev_alloc && !next_alloc)
    {
        delete_node(bp);
        delete_node(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    }

    else if (!prev_alloc && next_alloc)
    {
        delete_node(bp);
        delete_node(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
    }
    else
    {
        delete_node(bp);
        delete_node(PREV_BLKP(bp));
        delete_node(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); 
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
    }           
    
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    insert_node(bp);
    DBG_PRINTF("after coalesce %u\n",(unsigned int)bp);
    CHECKHEAP();
    return bp;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size = (words % 2)? (words + 1) * WSIZE : words * WSIZE;

    /* Allocate even number of words to maintain alignment */    
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    PUT(HDRP(bp), PACK(size, 0)); /* free block header */
    PUT(FTRP(bp), PACK(size, 0)); /* free block footper */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* new epilogue header */
    // DBG_PRINTF("extend_heap, before insert \n");
    // CHECKHEAP();
    insert_node(bp);
   
    /* Coalesce if the previous block was free */
    return coalesce(bp); 
    return bp;
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(16*WSIZE)) == (void *) -1)
        return -1;
    PUT(heap_listp + 0*WSIZE, 0); /* class 0: 1-2 WSIZE */
    PUT(heap_listp + 1*WSIZE, 0); /* class 1: 3 */
    PUT(heap_listp + 2*WSIZE, 0); /* class 2: 4 */
    PUT(heap_listp + 3*WSIZE, 0); /* class 3: <8 */
    PUT(heap_listp + 4*WSIZE, 0); /* class 4: <16 */
    PUT(heap_listp + 5*WSIZE, 0); /* class 5: <32 */
    PUT(heap_listp + 6*WSIZE, 0); /* class 6: <64 */
    PUT(heap_listp + 7*WSIZE, 0); /* class 7: <128 */
    PUT(heap_listp + 8*WSIZE, 0); /* class 8: <256 */
    PUT(heap_listp + 9*WSIZE, 0); /* class 9: <512 */
    PUT(heap_listp + 10*WSIZE, 0); /* class 10: <1024 */
    PUT(heap_listp + 11*WSIZE, 0); /* class 11: <2048 */
    PUT(heap_listp + 12*WSIZE, 0); /* class 12: >=2048 */
    // PUT(heap_listp + 13*WSIZE, 0); /* class 13: >=4096 */
    PUT(heap_listp + 13*WSIZE, PACK(DSIZE, 1)); /* prologue header */
    PUT(heap_listp + 14*WSIZE, PACK(DSIZE, 1)); /* prologue footer */
    PUT(heap_listp + 15*WSIZE, PACK(0, 1)); /* epilogue header */
    DBG_PRINTF("heap_listp init %u\n",(unsigned int)heap_listp);
    CHECKHEAP();
    
    /* Extend the heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

inline int get_class(size_t aligend_size)
{
    aligend_size /= WSIZE;
    if(aligend_size <= 2)
        return 0;
    if(aligend_size == 3)
        return 1;
    if(aligend_size == 4)
        return 2;
    int acc = 0;
    while(aligend_size && acc < CLASSNUM)
    {
        acc++;
        aligend_size = aligend_size >> 1;
    }
    return acc;
}

/* first fit search */
static void *find_fit(size_t aligned_size)
{
    // //printf("\nfind fit heaplistp %u, SUCC %u GETSUCC %u\n",heap_listp,SUCCP(heap_listp),SUCC_BLKP(heap_listp));
    char *bp;
    // //printf("find fit %u bp %u HDR%u\n",aligned_size,(unsigned int)bp, (HDRP(bp)));
    for(int class = get_class(aligned_size); class <= CLASSNUM; ++class)
    {
        bp = *(char **)(heap_listp + class * WSIZE);
        while(bp)
        {
            if(GET_SIZE(HDRP(bp)) >= aligned_size)
                return bp;
            else
                bp = SUCC_BLKP(bp);
        }
    }
    return NULL;
}

static void place(void *bp, size_t aligned_size)
{
    //printf("place %u, %u, HDR %u SIZE %u ",bp,aligned_size,HDRP(bp),GET_SIZE(HDRP(bp)));
    //printf("brk %u\n",mem_sbrk(0));
    size_t old_size = GET_SIZE(HDRP(bp));
    size_t remainder_size = old_size - aligned_size;
    char *succp = SUCC_BLKP(bp);
    char *predp = PRED_BLKP(bp);

    if(remainder_size < 4 * WSIZE)
    {
        UPDATE_SUCC(predp, succp);
        UPDATE_PRED(succp, predp);
        PUT(HDRP(bp), PACK(old_size, 1));
        PUT(FTRP(bp), PACK(old_size, 1));
    }
    else /* need splitting */
    {
        DBG_PRINTF("split %u, succp %u, predp %u\n",(unsigned int)bp,(unsigned int)succp,(unsigned int)predp);
        PUT(HDRP(bp), PACK(aligned_size, 1));
        PUT(FTRP(bp), PACK(aligned_size, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(remainder_size, 0));
        PUT(FTRP(bp), PACK(remainder_size, 0));
        UPDATE_SUCC(bp, succp);
        UPDATE_PRED(bp, predp);
        UPDATE_SUCC(predp, bp);
        UPDATE_PRED(succp, bp);
        delete_node(bp);
        insert_node(bp);
    }
}
/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    char *bp;
    size_t extend_size;
    DBG_PRINTF("malloc %u\n",size);
    // CHECKHEAP();
    if(size == 0)
        return NULL;

    if(size <= DSIZE)
        size = 2 * DSIZE;
    else
        size = ALIGN(size + DSIZE);
    DBG_PRINTF("asize %u, find_fit %u\n",size, find_fit(size));
    if ((bp = find_fit(size)) != NULL)
    {
        place(bp, size);
    }
    else
    {
        extend_size = size > CHUNKSIZE? size : CHUNKSIZE;
        if ((bp = extend_heap(extend_size/WSIZE)) == NULL)
            return NULL;
        place(bp, size);
    }
    DBG_PRINTF("after malloc\n");
    CHECKHEAP();
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    DBG_PRINTF("free %u\n",ptr);
    CHECKHEAP();
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    insert_node(ptr);
    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = GET_SIZE(HDRP(ptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

