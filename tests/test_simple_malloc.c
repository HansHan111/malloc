#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#define ALLOC_SIZE 160
#define CHUNK_SIZE 4096
#define NMEMB 21
#define REALLOC_SIZE_1 180
#define REALLOC_SIZE_2 300

/* This test checks to ensure that the program break does not move
 * between two allocations of the same size.  Note that this test WILL
 * PASS with the given allocator, but that the given allocator does NOT
 * meet the requirements of this assignment.  This test should, however,
 * help you debug your own allocator and think of ways to write your own
 * tests for your allocator. */
int main(int argc, char *argv[])
{
    void *brk1 = sbrk(0);
    if (brk1 == NULL)
    {
        fprintf(stderr, "\nsbrk() failed");
        return 1;
    }
    /* First request an allocation of ALLOC_SIZE bytes to cause the
     * allocator to fetch some memory from the OS and "prime" this
     * allocation size block. */
    void *p1 = malloc(ALLOC_SIZE);
    fprintf(stderr, "\n p1: %p\n", p1);

    /* Call sbrk(0) to retrieve the current program break without
     * changing it.  When we allocation again, this value should not
     * change, because the allocator should not fetch more memory from
     * the OS. */
    void *brk2 = sbrk(0);
    /* Request a second allocation of ALLOC_SIZE, which should simply
     * take an allocation block off the list for ALLOC_SIZE. */
    void *p2 = malloc(ALLOC_SIZE);
    fprintf(stderr, "\n p2: %p\n", p2);
    /* Compare the new program break to the old and make sure that it
     * has not changed. */
    if (brk2 != sbrk(0))
    {
        fprintf(stderr, "\n program fetch more memory from os.");
        fprintf(stderr, "\n brk2: %p, p2: %p, sbrk(0): %p", brk2, p2, sbrk(0));
        return 1;
    }

    // malloc Large memory
    void *p3 = malloc(CHUNK_SIZE);
    fprintf(stderr, "\n p3: %p\n", p3);

    /* Compare the new program break to the old and make sure that it
     * has not changed. */
    if (brk2 != sbrk(0))
    {
        fprintf(stderr, "\n program fetch more memory from os.");
        fprintf(stderr, "\n brk2: %p, p3: %p, sbrk(0): %p", brk2, p3, sbrk(0));
        return 1;
    }

    // free memory
    free(p1);

    // free bulk memory
    free(p3);

    // Test calloc
    void *p4 = calloc(NMEMB, sizeof(int));

    // check calloc() set memory to zero
    for (int i = 0; i < NMEMB; i++)
    {
        int element = *((int *)(p4) + i);
        if (*((int *)(p4) + i) != 0)
        {
            fprintf(stderr, "\n calloc() does not set memory to zero");
            fprintf(stderr, "\n p4: %p, index: %d, value: %d", p4, i, element);
            return 1;
        }
    }

    // Test realloc()
    for (int i = 0; i < ALLOC_SIZE; i++)
    {
        *(char *)(p2 + i) = 0xff;
    }
    void *p5 = realloc(p2, REALLOC_SIZE_1);

    // check Extend allocation
    if (p5 != p2)
    {
        fprintf(stderr, "\nrealloc() could not extend(and reduce) allocations\n");
        return 1;
    }

    void *p6 = realloc(p2, REALLOC_SIZE_2);
    fprintf(stderr, "\np2: %p, p6: %p", p2, p6);

    // check realloc copies memory
    for (int i = 0; i < ALLOC_SIZE; i++)
    {
        if (*(char *)(p6 + i) != (char)0xff)
        {
            fprintf(stderr, "\nrealloc() does not copy memory data");
            fprintf(stderr, "\nindex: %d, val: 0x%02hhx\n", i, *(char *)(p6 + i) & 0xff);
            return 1;
        }
    }

    return 0;
}
