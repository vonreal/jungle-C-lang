/*
 * memlib.c - a module that simulates the memory system.  Needed because it
 *            allows us to interleave calls from the student's malloc package
 *            with the system's malloc package in libc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>

#include "memlib.h"
#include "config.h"

/* private variables */
static char *mem_start_brk; /* points to first byte of heap */
static char *mem_brk;       /* points to last byte of heap */
static char *mem_max_addr;  /* largest legal heap address */

/*
 * mem_init - initialize the memory system model

 힙에 가용한 가상메모리를 큰 더블 워드로 정렬된 바이트의 배열로 모델한 것
 mem_heap = mem_start_brk과 mem_brk 사이의 바이트들은 할당된 가상메모리
 mem_brk 이후에의 바이트들은 미할당 가상메모리
 */
void mem_init(void)
{
    /* allocate the storage we will use to model the available VM */
    if ((mem_start_brk = (char *)malloc(MAX_HEAP)) == NULL)
    {
        fprintf(stderr, "mem_init_vm: malloc error\n");
        exit(1);
    }

    mem_max_addr = mem_start_brk + MAX_HEAP; /* max legal heap address */
    mem_brk = mem_start_brk;                 /* heap is empty initially */
}

/*
 * mem_deinit - free the storage used by the memory system model
 */
void mem_deinit(void)
{
    free(mem_start_brk);
}

/*
 * mem_reset_brk - reset the simulated brk pointer to make an empty heap
 */
void mem_reset_brk()
{
    mem_brk = mem_start_brk;
}

/*
 * mem_sbrk - simple model of the sbrk function. Extends the heap
 *    by incr bytes and returns the start address of the new area. In
 *    this model, the heap cannot be shrunk.
 *
 * 힙을 Incr 바이트만큼 확장합니다. 여기서 Incr은 양의 정수입니다.
 * 새롭게 할당된 영역의 첫 번째 바이트를 가리키는 포인터를 반환합니다. (표준 unix 시스템 호출인 sbrk와 유사하지만, 오직 양수값만 인자로 받습니다.)
 */
void *mem_sbrk(int incr)
{
    char *old_brk = mem_brk;

    if ((incr < 0) || ((mem_brk + incr) > mem_max_addr))
    {
        errno = ENOMEM;
        fprintf(stderr, "ERROR: mem_sbrk failed. Ran out of memory...\n");
        return (void *)-1;
    }
    mem_brk += incr;
    return (void *)old_brk;
}

/*
 * mem_heap_lo - return address of the first heap byte

 힙의 첫 번째 바이트를 가리키는 포인터를 반환합니다.
 */
void *mem_heap_lo()
{
    return (void *)mem_start_brk;
}

/*
 * mem_heap_hi - return address of last heap byte

 힙의 마지막 바이트를 가리키는 포인터를 반환합니다.
 */
void *mem_heap_hi()
{
    return (void *)(mem_brk - 1);
}

/*
 * mem_heapsize() - returns the heap size in bytes

 현재 힙의 크기를 바이트 단위로 반환합니다.
 */
size_t mem_heapsize()
{
    return (size_t)(mem_brk - mem_start_brk);
}

/*
 * mem_pagesize() - returns the page size of the system

 시스템의 페이지 크기(바이트 단위)를 반환합니다. (리눅스 시스템에서는 보통 4096)
 */
size_t mem_pagesize()
{
    return (size_t)getpagesize();
}
