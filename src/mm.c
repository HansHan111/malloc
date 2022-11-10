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


#define DEBUG

/* Define DEBUG_MESSAGE macro
 * this macro print message when DEBUG is defined*/
#ifdef DEBUG
    #define DEBUG_MESSAGE(...) fprintf(stderr, __VA_ARGS__)
#else 
    #define DEBUG_MESSAGE(...)
#endif

/* When requesting memory from the OS using sbrk(), request it in
 * increments of CHUNK_SIZE. */
#define CHUNK_SIZE (1<<12)

/* Double word size. That is the size of block header.*/
#define DSIZE 8

/* Pack header data from size and allocation flag. */
#define PACK(size, alloc) ((size) | (alloc))

/* Get header data from header pointer */
#define GET(p) (*(p))
/* Put header data (val) to header pointer */
#define PUT(p, val) (*(p) = (val))

/* Get block size from header pointer */
#define GET_SIZE(p) (GET(p) & ~0x7)
/* Get block allocation flag from header pointer*/
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Get next block header pointer from header pointer */
#define NEXT_HEAD(p) ((Header *)((char *)(p) + GET_SIZE(p)))

/* Get predecessor free block header of explicit free lists from free block header */
#define PREV_FREE_HEAD(p)(*(HEADER **)((char *)(p) + DSIZE))
/* Get successor free block header of explicit free lists from free block header */
#define NEXT_FREE_HEAD(p)(*(HEADER **)((char *)(p) + 2 * DSIZE))

/* Get the block header pointer from block pointer */
#define HDRP(bp) ((Header *)((char *)(bp) - DSIZE))
/* Get the block pointer from block header pointer */
#define BLKP(p)((char *)(p) + DSIZE)

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
static inline __attribute__((unused)) int block_index(size_t x) {
    if (x <= 8) {
        return 5;
    } else {
        return 32 - __builtin_clz((unsigned int)x + 7);
    }
}

/* define Header type. size is 8 bytes*/
typedef size_t Header;

/* define explicit free list Metadata structor.
 * pred: predecessor block header pointer 
 * succ: successor block header pointer 
 * ExplicitMetadata is saved after header 16 bytes in free block*/
struct ExplicitMeta{
    Header *pred;
    Header *succ;
};
/* define explicit Metadata type*/
typedef struct ExplicitMeta explicitMeta;

/* It represents explicit free lists
 * It points the last element of the linked list node data structor*/
static explicitMeta *explicit_free_lists = NULL;


extern void *sbrk(int increments);
int block_index(size_t x);
int init(void);
static void *find_free_block(size_t asize);
static Header *extend_heap();
static void place(Header* hp, size_t asize);

/* THIS MACRO PRINT BLOCK INFORMATION FOR DEBUG
 * hp: block header pointer
 * prefix: char *, prefix of each line.
 */
#ifdef DEBUG
    #define PRINT_BLOCK_INFO(hp, prefix) \
        DEBUG_MESSAGE("\n%s", prefix); \
        DEBUG_MESSAGE("Header Pointer: %p", hp); \
        DEBUG_MESSAGE("\n%s", prefix); \
        DEBUG_MESSAGE("Block Pointer: %p", BLKP(hp)); \
        DEBUG_MESSAGE("\n%s", prefix); \
        DEBUG_MESSAGE("Block Size: %ld", GET_SIZE(hp))
#else
    #define PRINT_BLOCK_INFO(hp, prefix)
#endif

/* Print all free block list for debug
 **/
#ifdef DEBUG
    static void printFreeBlockList(char *prefix){
        DEBUG_MESSAGE("\n%s", prefix);
        DEBUG_MESSAGE("------Free Block List --------");
        if(explicit_free_lists == NULL){
            DEBUG_MESSAGE("\n%s", prefix);
            DEBUG_MESSAGE("\tNo Free Block");
            DEBUG_MESSAGE("\n%s", prefix);
            DEBUG_MESSAGE("-----------------------------");
            return;
        }
        Header *p, *prevp;
        p = HDRP(explicit_free_lists);
        prevp = explicit_free_lists -> pred;
        while(prevp != p){
            DEBUG_MESSAGE("\n%s", prefix);
            DEBUG_MESSAGE("******FREE BLOCK******");
            PRINT_BLOCK_INFO(p, prefix);
            DEBUG_MESSAGE("\n%s", prefix);
            DEBUG_MESSAGE("***********************");
            p = prevp;
            explicitMeta * exMeta = (explicitMeta*) BLKP(p);
            prevp = exMeta -> pred;
        }
        DEBUG_MESSAGE("\n%s", prefix);
        DEBUG_MESSAGE("******FREE BLOCK******");
        PRINT_BLOCK_INFO(p, prefix);
        DEBUG_MESSAGE("\n%s", prefix);
        DEBUG_MESSAGE("***********************");
        DEBUG_MESSAGE("\n%s", prefix);
        DEBUG_MESSAGE("------End Free Block List------");
    }
#endif


/*
 * You must implement malloc().  Your implementation of malloc() must be
 * the multi-pool allocator described in the project handout.
 */
void *malloc(size_t size) {
    DEBUG_MESSAGE("\n-------Start Malloc------");
    #ifdef DEBUG
        printFreeBlockList("\t");
    #endif
    //if size is zero
    if(size == 0) return NULL;
    // calculate desire align size
    size_t asize = 1 << block_index(size);
    Header * hp;

    DEBUG_MESSAGE("\n");
    DEBUG_MESSAGE("\n\tSize: %ld", size);
    DEBUG_MESSAGE("\n\tAlign Block Size: %ld", asize);

    //if size is large
    if(size > CHUNK_SIZE - DSIZE){
        DEBUG_MESSAGE("\n");
        DEBUG_MESSAGE("\n\tBulk allocations are used for large allocations");
        //using bulk_alloc
        asize = DSIZE * ((size + (DSIZE) + (DSIZE + 1)) / DSIZE);
        hp =(Header *) bulk_alloc(asize);
        //Set header
        PUT(hp, PACK(asize, 1));
        DEBUG_MESSAGE("\n\t*********Malloc Result*********");
        PRINT_BLOCK_INFO(hp, "\t");
        DEBUG_MESSAGE("\n\t*********************************");
        DEBUG_MESSAGE("\n--------------End Malloc-------------");
        //return block pointer
        return BLKP(hp);
    }
    
    DEBUG_MESSAGE("\n");
    DEBUG_MESSAGE("\n\t...Find Fit Free Block");

    //find free block to fit align size
    if((hp = find_free_block(asize)) != NULL){
        //if found fit block
        DEBUG_MESSAGE("\n\t\t*******Fit Free Block********");
        PRINT_BLOCK_INFO(hp, "\t\t");
        DEBUG_MESSAGE("\n\t\t*****************************");

        DEBUG_MESSAGE("\n\t...Place Align Size Block");
        //place align size block to free block
        place(hp, asize);

        DEBUG_MESSAGE("\n");
        DEBUG_MESSAGE("\n\t#########Malloc Result###########");
        #ifdef DEBUG
            printFreeBlockList("\t");
        #endif
        DEBUG_MESSAGE("\n");
        DEBUG_MESSAGE("\n\t**********Result Block***********");
        PRINT_BLOCK_INFO(hp, "\t");
        DEBUG_MESSAGE("\n\t*********************************");
        DEBUG_MESSAGE("\n--------------End Malloc--------------------");
        //return block pointer
        return BLKP(hp);
    }

    //if not found free block to fit align size
    DEBUG_MESSAGE("\n\t\tNo Fit Free Block");
    DEBUG_MESSAGE("\n");
    DEBUG_MESSAGE("\n\t...Extend Heap");
    //extend heap;
    if((hp = extend_heap()) == NULL) return NULL;

    DEBUG_MESSAGE("\n");
    DEBUG_MESSAGE("\n\t\t*******Extended Block******");
    PRINT_BLOCK_INFO(hp, "\t\t");
    DEBUG_MESSAGE("\n\t\t***************************");
    DEBUG_MESSAGE("\n\t...Place Align Size Block");
    //place align size block to extended block
    place(hp, asize);

    DEBUG_MESSAGE("\n");
    DEBUG_MESSAGE("\n\t#########Malloc Result###########");
    #ifdef DEBUG
        printFreeBlockList("\t");
    #endif
    DEBUG_MESSAGE("\n");
    DEBUG_MESSAGE("\n\t**********Result Block***********");
    PRINT_BLOCK_INFO(hp, "\t");
    DEBUG_MESSAGE("\n\t*********************************");
    DEBUG_MESSAGE("\n--------------End Malloc--------------------");
    //return block pointer
    return BLKP(hp);
}

/* Split Free block to two Free block
*/
static Header * split_free_block(Header * hp){
    size_t size = GET_SIZE(hp);
    //if size < 32
    if (size <= 1 << 5){
        // do nothing
        return hp;
    }
    // calculate half of size
    size_t half_size = size >> 1; 

    
    //get explicit metadata of block
    explicitMeta *exMeta = (explicitMeta *)BLKP(hp);
    //get successor block header pointer
    Header *succ = exMeta -> succ;
    //get explicit metadata of successor block
    explicitMeta *succExMeta = (explicitMeta *)BLKP(succ);
    // set block size as half of origin size
    PUT(hp, PACK(half_size, 0));
    // get next half block header pointer
    Header *next_hp = NEXT_HEAD(hp);
    // set next block size as half of origin size 
    PUT(next_hp, PACK(half_size, 0));
    // get next block explicit Metadata
    explicitMeta *nextExMeta = (explicitMeta *)BLKP(next_hp);
    

    //Reset explicit Metadatas
    //if successor is not origin block
    if(succ != hp){
        // set successor of first half block as second half block
        exMeta -> succ = next_hp;
        // set predecessor of second half block as first half block
        nextExMeta -> pred = hp;
        //set successor of second half block as successor of origin block
        nextExMeta -> succ = succ;
        //set predecessor of origin successor as second half block
        succExMeta -> pred = next_hp;
    //else
    }else{ // it means origin block is the last node of explicit free lists
        //set successor of first half block as second half block
        exMeta -> succ = next_hp;
        // set predecessor of second half block as first half block
        nextExMeta -> pred = hp;
        // set successor of second half block as itself(second half block)
        nextExMeta -> succ = next_hp;
        // set explicit_free_lists as metadata of second half block
        //(setting last node of explicit free lists)
        explicit_free_lists = nextExMeta;
    }

    DEBUG_MESSAGE("\n\t\t\tSplit %ld bytes block at %p to two %ld bytes blocks at %p, %p", size, hp, half_size, hp, next_hp);
    // return first half block
    return hp;
}

static void place(Header* hp, size_t asize){
    DEBUG_MESSAGE("\n\t\t---------start place------------");
    size_t size = GET_SIZE(hp);
    //split block until first block equals asize;
    while(size > asize){
        hp = split_free_block(hp);
        size = GET_SIZE(hp);
    }
    
    // get explicit metadata
    explicitMeta *exMeta = (explicitMeta *)BLKP(hp);
    // get predecessor block header pointer
    Header *pred = exMeta -> pred;
    // get successor block header pointer
    Header *succ = exMeta -> succ;
    // get explicit metadata of predecessor
    explicitMeta *predExMeta = (explicitMeta *)BLKP(pred);
    // get explicit metadata of successor
    explicitMeta *succExMeta = (explicitMeta *)BLKP(succ);

    DEBUG_MESSAGE("\n");
    DEBUG_MESSAGE("\n\t\t\t...Set Allocation Flag as 1");
    // set header to allocated
    PUT(hp, PACK(asize, 1));

    DEBUG_MESSAGE("\n\t\t\t...Set Explicit Metadata");
    // reset explicit metadatas
    //if predecessor not equal origin block and successor not equal origin block
    if(pred != hp && succ != hp){
        //set successor of predecessor as origin successor
        predExMeta -> succ = succ;
        succExMeta -> pred = pred;
    //if predecessor not equal origin block and successor equals origin block
    }else if(pred != hp && succ == hp){ //it means origin block is the last node of explicit free lists
        //set successor of predecessor as itself(origin predecessor)
        predExMeta -> succ = pred;
        // set explicit_free_lists as explicit metadata of origin predecessor
        explicit_free_lists = predExMeta;
    //if predecessor equals origin block and successor not equals predecessor    
    }else if(pred == hp && succ != hp){ //it means origin block is the first node of explicit free lists
        // set predecessor of successor as itself(origin successor);
        succExMeta -> pred = succ;
    //else
    }else{ // it means there was only one free block
        // set explicit_free_lists as Null
        explicit_free_lists = NULL;
    }
    
    DEBUG_MESSAGE("\n\t\t--------End Place-------------");
}

static void *find_free_block(size_t asize){
    Header *p, *prevp;
    //if no free block
    if(explicit_free_lists == NULL){
        return NULL;
    }
    //initial p as last node of explicit free lists
    p = explicit_free_lists -> succ;
    // set prevp as predecessor of p
    prevp = explicit_free_lists -> pred;
    // iterate over explicit free lists
    while(prevp != p){
        //if block size >= asize
        if(GET_SIZE(p) >= asize){
            //return p
            return p;       
        }
        //else update p and prevp
        p = prevp;
        explicitMeta * exMeta = (explicitMeta*)BLKP(p);
        prevp = exMeta -> pred;
    }
    //check first node of explicit free lists
    if(GET_SIZE(p) >= asize){
        return p;
    }
    // return Null
    return NULL;
}

static Header *extend_heap(){
    DEBUG_MESSAGE("\n\t\t-------------Start Extend Heap---------------");
    DEBUG_MESSAGE("\n\t\t\tsbrk(0) : %p", sbrk(0));
    DEBUG_MESSAGE("\n\t\t\t...Create new block");

    //request new CHUNK_SIZE memory
    void *p = sbrk(CHUNK_SIZE);
    if(p ==(void *) -1){
        return NULL;
    }
    // create new free block with CHUNK_SIZE
    Header *hp = (Header *)p;
    PUT(hp, PACK(CHUNK_SIZE, 0));
    
    PRINT_BLOCK_INFO(hp, "\t\t\t\t");
    DEBUG_MESSAGE("\n\t\t\t...Update Explicit Free Lists");
    // get explicit metadata of new block
    explicitMeta* exMeta = (explicitMeta *)BLKP(hp);
    // set explicit metadata
    //if explicit_free_lists == NULL
    if(explicit_free_lists == NULL){ //there's no free block
        // set predecessor and successor as itself
        exMeta -> pred = hp;
        exMeta -> succ = hp;
        // set explicit_free_list as explicit metadata of new block
        explicit_free_lists = exMeta;
    //else
    }else{
        //add new node to the last of explicit free lists
        //set predecessor of new block as origin last free block
        exMeta -> pred = explicit_free_lists -> succ;
        //set successor of origin last free block as new block
        explicit_free_lists -> succ = hp;
        //set successor of new block as itself
        exMeta-> succ = hp;
        // set explicit_free_lists as explicit metadata of new block
        explicit_free_lists = exMeta;
    }
    #ifdef DEBUG
        printFreeBlockList("\t\t\t\t");
    #endif

    DEBUG_MESSAGE("\n\t\t------------End Extend Heap----------------");
    //return block header pointer
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
void *calloc(size_t nmemb, size_t size) {
    DEBUG_MESSAGE("\n----------Start Calloc-------------");
    //calculate total size
    size_t total_size = nmemb * size;
    DEBUG_MESSAGE("\n...Malloc()");
    //allocate with malloc function
    void *ptr = malloc(total_size);
    DEBUG_MESSAGE("\n");
    DEBUG_MESSAGE("\n...Set Memory to Zero");
    // set 0 of all memory
    memset(ptr, 0, total_size);
    DEBUG_MESSAGE("\n----------End Calloc----------------\n");
    //return ptr
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
void *realloc(void *ptr, size_t size) {
    DEBUG_MESSAGE("\n--------Start Realloc------------");
    //get block header of ptr
    Header *hp = HDRP(ptr);
    //get block size
    size_t block_size = GET_SIZE(hp);
    DEBUG_MESSAGE("\n...Check Extend Or Reduce Allocations");
    // block size is enough large than size return origin block
    if(block_size <= CHUNK_SIZE && block_size - DSIZE > size){
        DEBUG_MESSAGE("\n\tExtend(Reduce) Allocations");
        DEBUG_MESSAGE("\n------------End Realloc-----------\n");
        return ptr;
    } 
    //else
    //malloc new block
    DEBUG_MESSAGE("\n...malloc new memory");
    void *new_ptr = malloc(size);
    // copy origin data to new data
    DEBUG_MESSAGE("\n");
    DEBUG_MESSAGE("\n...Copy Data");
    memcpy(new_ptr, ptr, block_size - DSIZE);
    // free origin block
    DEBUG_MESSAGE("\n...Free Original memory");
    free(ptr);
    // return new block
    return new_ptr;    
}

/*
 * You should implement a free() that can successfully free a region of
 * memory allocated by any of the above allocation routines, whether it
 * is a pool- or bulk-allocated region.
 *
 * The given implementation does nothing.
 */
void free(void *ptr) {
    DEBUG_MESSAGE("\n---------Start Free---------------");
    //get block header
    Header* hp = HDRP(ptr);
    DEBUG_MESSAGE("\n\t**********Block Info*************");
    PRINT_BLOCK_INFO(hp, "\t");
    DEBUG_MESSAGE("\n\t*********************************");
    // get block size
    size_t size = GET_SIZE(hp);
    // if block is large
    if(size > CHUNK_SIZE - DSIZE){
        DEBUG_MESSAGE("\n\t...Using bulk_free()");
        // free using bulk_free and end function
        bulk_free(hp, size);
        DEBUG_MESSAGE("\n----------End Free----------\n");
        return;
    }
    // else
    // set allocation flag as 0
    DEBUG_MESSAGE("\n");
    DEBUG_MESSAGE("\n\t...Set Allocation Flag as 0");
    PUT(hp, PACK(size, 0));
    // set explicit Metadata and add to last of explicit free lists
    DEBUG_MESSAGE("\n\t...Set Explicit Metadata");
    // get explicit metadata of block
    explicitMeta *exMeta = (explicitMeta *)BLKP(hp);
    // if explicit_free_lists is null
    if(explicit_free_lists == NULL){ // it means there was no free block
        //set predecessor as itself
        exMeta -> pred = hp;
        // set successor as itself
        exMeta -> succ = hp;
        // set explicit_free_lists as explicit metadata of block
        explicit_free_lists = exMeta;
    }else{
        // set predecessor as origin last free block
        exMeta -> pred = explicit_free_lists -> succ;
        // set successor of origin last free block as current block
        explicit_free_lists -> succ = hp;
        // set successor of current block as itself
        exMeta -> succ = hp;
        // set explicit_free_lists as explicit metadata of current block
        explicit_free_lists = exMeta;
    }

    DEBUG_MESSAGE("\n");
    #ifdef DEBUG
        printFreeBlockList("\t");
    #endif
    DEBUG_MESSAGE("\n------------End Free-------------\n");
    return;
}

