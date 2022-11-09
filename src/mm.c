#include <string.h>
#include <stdio.h>

/* The standard allocator interface from stdlib.h.  These are the
 * functions you must implement, more information on each function is
 * found below. They are declared here in case you want to use one
 * function in the implementation of another. */
void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);

/* When requesting memory from the OS using sbrk(), request it in
 * increments of CHUNK_SIZE. */
#define CHUNK_SIZE (1 << 12)

/*
 * This function, defined in bulk.c, allocates a contiguous memory
 * region of at least size bytes.  It MAY NOT BE USED as the allocator
 * for pool-allocated regions.  Memory allocated using bulk_alloc()
 * must be freed by bulk_free().
 *
 * This function will return NULL on failure.
 */
extern void *bulk_alloc(size_t size);

/*
 * This function is also defined in bulk.c, and it frees an allocation
 * created with bulk_alloc().  Note that the pointer passed to this
 * function MUST have been returned by bulk_alloc(), and the size MUST
 * be the same as the size passed to bulk_alloc() when that memory was
 * allocated.  Any other usage is likely to fail, and may crash your
 * program.
 */
extern void bulk_free(void *ptr, size_t size);

/*
 * This function computes the log base 2 of the allocation block size
 * for a given allocation.  To find the allocation block size from the
 * result of this function, use 1 << block_size(x).
 *
 * Note that its results are NOT meaningful for any
 * size > 4088!
 *
 * You do NOT need to understand how this function works.  If you are
 * curious, see the gcc info page and search for __builtin_clz; it
 * basically counts the number of leading binary zeroes in the value
 * passed as its argument.
 */
static inline __attribute__((unused)) int block_index(size_t x)
{
    if (x <= 8)
    {
        return 5;
    }
    else
    {
        return 32 - __builtin_clz((unsigned int)x + 7);
    }
}
/* When requesting memory from the OS using sbrk(), request it in
 * increments of CHUNK_SIZE. */
#define CHUNK_SIZE (1 << 12)
#define DSIZE 8

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(p))
#define PUT(p, val) (*(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define NEXT_HEAD(p) ((Header *)((char *)p + GET_SIZE(p)))
#define PREV_FREE_HEAD(p) (*(HEADER **)((char *)p + DSIZE))
#define NEXT_FREE_HEAD(p) (*(HEADER **)((char *)p + 2 * DSIZE))

#define HDRP(bp) ((Header *)((char *)(bp)-DSIZE))
#define BLKP(p) ((char *)p + DSIZE)
/*
 * This function, defined in bulk.c, allocates a contiguous memory
 * region of at least size bytes.  It MAY NOT BE USED as the allocator
 * for pool-allocated regions.  Memory allocated using bulk_alloc()
 * must be freed by bulk_free().
 *
 * This function will return NULL on failure.
 */
extern void *bulk_alloc(size_t size);

/*
 * This function is also defined in bulk.c, and it frees an allocation
 * created with bulk_alloc().  Note that the pointer passed to this
 * function MUST have been returned by bulk_alloc(), and the size MUST
 * be the same as the size passed to bulk_alloc() when that memory was
 * allocated.  Any other usage is likely to fail, and may crash your
 * program.
 */
extern void bulk_free(void *ptr, size_t size);

/*
 * This function computes the log base 2 of the allocation block size
 * for a given allocation.  To find the allocation block size from the
 * result of this function, use 1 << block_size(x).
 *
 * Note that its results are NOT meaningful for any
 * size > 4088!
 *
 * You do NOT need to understand how this function works.  If you are
 * curious, see the gcc info page and search for __builtin_clz; it
 * basically counts the number of leading binary zeroes in the value
 * passed as its argument.
 */
static inline __attribute__((unused)) int block_index(size_t x)
{
    if (x <= 8)
    {
        return 5;
    }
    else
    {
        return 32 - __builtin_clz((unsigned int)x + 7);
    }
}

typedef size_t Header;

struct ExplicitMeta
{
    Header *pred;
    Header *succ;
};

typedef struct ExplicitMeta explicitMeta;

// static Header *freep = NULL;
static explicitMeta *explicit_free_lists = NULL;

extern void *sbrk(int increments);

int block_index(size_t x);
int init(void);
static void *find_free_block(size_t asize);
static Header *extend_heap();
static void place(Header *hp, size_t asize);

/*
 * You must implement malloc().  Your implementation of malloc() must be
 * the multi-pool allocator described in the project handout.
 */
void *malloc(size_t size)
{
    if (size == 0)
        return NULL;
    if (size > CHUNK_SIZE - DSIZE)
    {
        Header *hp = (Header *)bulk_alloc(size + DSIZE);
        PUT(hp, PACK(size, 1));
        return BLKP(hp);
    }
    size_t asize = 1 << block_index(size);
    Header *hp;
    if ((hp = find_free_block(asize)) != NULL)
    {
        place(hp, asize);
        return BLKP(hp);
    }

    if ((hp = extend_heap()) == NULL)
        return NULL;
    place(hp, asize);
    return BLKP(hp);
}

static void place(Header *hp, size_t asize)
{
    size_t size = GET_SIZE(hp);
    explicitMeta *exMeta = (explicitMeta *)BLKP(hp);
    Header *pred = exMeta->pred;
    Header *succ = exMeta->succ;
    explicitMeta *predExMeta = (explicitMeta *)BLKP(*pred);
    explicitMeta *succExMeta = (explicitMeta *)BLKP(*succ);
    if (size == asize)
    {
        PUT(hp, PACK(asize, 1));
        if (pred != hp && succ != hp)
        {
            predExMeta->succ = succ;
            succExMeta->pred = pred;
        }
        else if (pred != hp && succ == hp)
        {
            predExMeta->succ = pred;
            explicit_free_lists = predExMeta;
        }
        else if (pred == hp && succ != hp)
        {
            succExMeta->pred = succ;
        }
        else
        {
            explicit_free_lists = NULL;
        }
    }
    else
    {
        PUT(hp, PACK(asize, 1));
        Header *next_hp = NEXT_HEAD(hp);
        size -= asize;
        PUT(next_hp, PACK(size, 0));
        explicitMeta *nextExMeta = (explicitMeta *)BLKP(next_hp);
        if (pred != hp && succ != hp)
        {
            nextExMeta->pred = pred;
            nextExMeta->succ = succ;
            predExMeta->succ = next_hp;
            succExMeta->pred = next_hp;
        }
        else if (pred != hp && succ == hp)
        {
            predExMeta->succ = next_hp;
            nextExMeta->pred = pred;
            nextExMeta->succ = next_hp;
            explicit_free_lists = nextExMeta;
        }
        else if (pred == hp && succ != hp)
        {
            nextExMeta->pred = next_hp;
            nextExMeta->succ = succ;
            succExMeta->pred = next_hp;
        }
        else
        {
            nextExMeta->pred = next_hp;
            nextExMeta->succ = next_hp;
            explicit_free_lists = nextExMeta;
        }
    }
}

static void *find_free_block(size_t asize)
{
    Header *p, *prevp;
    if (explicit_free_lists == NULL)
    {
        return extend_heap();
    }
    p = explicit_free_lists->succ;
    prevp = explicit_free_lists->pred;
    while (prevp != p)
    {
        if (GET_SIZE(p) >= asize)
        {
            return p;
        }
        p = prevp;
        explicitMeta *exMeta = (explicitMeta *)BLKP(p);
        prevp = exMeta->pred;
    }
    if (GET_SIZE(p) >= asize)
    {
        return p;
    }
    return NULL;
}

static Header *extend_heap()
{
    void *p = sbrk(CHUNK_SIZE);

    if (p == (void *)-1)
    {
        return NULL;
    }

    Header *hp = (Header *)p;
    PUT(hp, PACK(CHUNK_SIZE, 0));
    explicitMeta *exMeta = (explicitMeta *)BLKP(hp);
    if (explicit_free_lists == NULL)
    { // there's no free block
        exMeta->pred = hp;
        exMeta->succ = hp;
        explicit_free_lists = exMeta;
    }
    else
    {
        exMeta->pred = explicit_free_lists->succ;
        explicit_free_lists->succ = hp;
        exMeta->succ = hp;
        explicit_free_lists = exMeta;
    }
    return hp;
}

/*
 * You must also implement calloc().  It should create allocations
 * compatible with those created by malloc().  In particular, any
 * allocations of a total size <= 4088 bytes must be pool allocated,
 * while larger allocations must use the bulk allocator.
 *
 * calloc() (see man 3 calloc) returns a cleared allocation large enough
 * to hold nmemb elements of size size.  It is cleared by setting every
 * byte of the allocation to 0.  You should use the function memset()
 * for this (see man 3 memset).
 */
void *calloc(size_t nmemb, size_t size)
{
    size_t total_size = nmemb * size;
    void *ptr = malloc(total_size);
    memset(ptr, 0, total_size);
    return ptr;
}

/*
 * You must also implement realloc().  It should create allocations
 * compatible with those created by malloc(), honoring the pool
 * alocation and bulk allocation rules.  It must move data from the
 * previously-allocated block to the newly-allocated block if it cannot
 * resize the given block directly.  See man 3 realloc for more
 * information on what this means.
 *
 * It is not possible to implement realloc() using bulk_alloc() without
 * additional metadata, so the given code is NOT a working
 * implementation!
 */
void *realloc(void *ptr, size_t size)
{
    Header hp = HDRP(ptr);
    size_t block_size = GET_SIZE(hp);
    if (block_size > size)
        return ptr;
    void *new_ptr = malloc(size);
    memcpy(new_ptr, ptr, block_size - DSIZE);
    free(ptr);
    return new_ptr;
}

/*
 * You should implement a free() that can successfully free a region of
 * memory allocated by any of the above allocation routines, whether it
 * is a pool- or bulk-allocated region.
 *
 * The given implementation does nothing.
 */
void free(void *ptr)
{
    Header *hp = HDRP(ptr);
    size_t size = GET_SIZE(hp);
    if (size > CHUNK_SIZE - DSIZE)
    {
        bulk_free(hp, size);
    }
    PUT(hp, PACK(size, 0));
    explicitMeta *exMeta = (explicitMeta *)BLKP(hp);
    Header *last_free = explicit_free_lists->succ;
    explicit_free_lists->succ = hp;
    exMeta->pred = last_free;
    exMeta->succ = hp;
    explicit_free_lists = exMeta;
    return;
}
