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
    "Naco",
    /* First member's full name */
    "nacotti",
    /* First member's email address */
    "vonreal20@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
#define WORD_SIZE 4

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define CHUNKSIZE (1 << 12)

#define PUT(bp, val) (*(unsigned int *)(bp) = (val))

#define PATCH(size, alloc) ((size) | (alloc))

/* block pointer의 헤더의 시작주소를 반환한다. bp는 항상 페이로드의 시작주소를 가르키고 있다. */
#define HDRP(bp) (char *)(bp) - WORD_SIZE
#define FDRP(bp) (char *)(bp) - ALIGNMENT

#define NEXT_BLOCK_PTR(bp) ((char *)(bp) + (GET_SIZE(HDRP(bp))))
#define PREV_BLOCK_PTR(bp) ((char *)(bp) - (GET_SIZE(FDRP(bp))))

#define GET_SIZE(p) *(unsigned int *)(p) & ~0x07
#define GET_ALLOC(p) *(unsigned int *)(p) & 0x01

char *heap_listp;

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    // 1. 4 word 초기화하기
    printf("before heap: %p\n", mem_heap_hi());
    if ((heap_listp = mem_sbrk(4 * WORD_SIZE)) == (void *)-1)
        return -1;
    printf("after heap: %p\n", mem_heap_hi());

    PUT(heap_listp, 0);                                         // 패딩
    PUT(heap_listp + (1 * WORD_SIZE), PATCH(2 * WORD_SIZE, 1)); // 프롤로그 > 헤더
    PUT(heap_listp + (2 * WORD_SIZE), PATCH(2 * WORD_SIZE, 1)); // 프롤로그 > 푸터
    PUT(heap_listp + (3 * WORD_SIZE), PATCH(0, 1));             // 에필로그 > 헤더

    heap_listp += (2 * WORD_SIZE); // 푸터의 시작주소로 이동

    if (heap_listp = extend_heap(CHUNKSIZE / WORD_SIZE) == NULL) // 4096
        return -1;

    return 0;
}

/*
 * extend_heap - CHUNKSIZE만큼 늘린다.
 */
void *extend_heap(size_t words)
{
    size_t asize = WORD_SIZE * ALIGN(words);
    void *bp;
    if ((bp = mem_sbrk(asize)) == (void *)-1)
        return NULL;

    PUT(HDRP(bp), PATCH(asize, 0));         // 기존 에필로그 헤더 덮어씌우기
    PUT(FDRP(bp + asize), PATCH(asize, 0)); // 푸터로 푸터 값 넣기
    PUT(HDRP(bp + asize), PATCH(0, 1));     // 에필로그 헤더 만들기
    // PUT(HDRP((bp + asize) - 8), PATCH(asize, 0)); // 푸터 만들기

    return coalesing(bp);
}

/*
 * coalesing - free블록을 합쳐준다.
 */
void *coalesing(void *bp)
{
    if (bp == NULL)
        return NULL;

    void *prev_block = PREV_BLOCK_PTR(bp);
    void *next_block = NEXT_BLOCK_PTR(bp);

    printf("prev_block: %d\n", GET_ALLOC(HDRP(prev_block)));
    printf("next_block: %d\n", GET_ALLOC(HDRP(next_block)));

    // case1 앞 뒤 모두 할당중일때
    if (GET_ALLOC(HDRP(prev_block)) && GET_ALLOC(HDRP(next_block)))
        return bp;
    // case2 앞만 할당중 (뒤가 free)
    // else if (GET_ALLOC(prev_block))
    // {
    //     // 현재 블록과 뒷 블록의 사이즈를 합친다.
    //     // 앞의 블록의 헤더와 푸터를 변형한다.
    //     size_t aszie = GET_SIZE(HDRP(bp)) + GET_SIZE(HDRP(next_block));

    //     PUT(HDRP(bp), PATCH(aszie, 0));
    //     PUT(FDRP(next_block), PATCH(aszie, 0));

    //     return bp;
    // }
    // // case3 뒤만 할당중 (앞이 free)
    // else if (GET_ALLOC(next_block))
    // {
    //     // 현재 블록과 뒷 블록의 사이즈를 합친다.
    //     // 앞의 블록의 헤더와 푸터를 변형한다.
    //     size_t aszie = GET_SIZE(HDRP(bp)) + GET_SIZE(FDRP(prev_block));

    //     PUT(HDRP(prev_block), PATCH(aszie, 0));
    //     PUT(FDRP(bp), PATCH(aszie, 0));

    //     return prev_block;
    // }
    // // case4 앞 뒤 free
    // else
    // {
    //     size_t aszie = GET_SIZE(HDRP(bp)) + GET_SIZE(FDRP(prev_block)) ;

    //     PUT(HDRP(prev_block), PATCH(aszie, 0));
    //     PUT(FDRP(bp), PATCH(aszie, 0));

    //     return prev_block;
    // }

    return NULL;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1)
        return NULL;
    else
    {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
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
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}