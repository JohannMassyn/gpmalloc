/* General Purpose Memory Allocator (gpmalloc)
 * gpmalloc.c
 *
 * Copyright (C) 2018
 * All Rights Reserved
 */

/* -------------------- Options -------------------- */
#define DEBUG
#define USE_HEADER

//Locks
#define USE_LOCK
//#define USE_LOCK_SPIN

//Tree table
#define TABLE_SIZE 4096

//Defult Pagesize
#define PAGESIZE_DEFAULT 4096
#define PAGE_MIN_ALLOC 1

#define USE_SBRK

/* -------------------- Headers -------------------- */

#ifdef USE_HEADER
	#include "gpmalloc.h"
#endif

//Debug headers
#ifdef DEBUG
	#include <stdio.h>
#endif //DEBUG

#include <stdbool.h>
#include <memory.h>
#include <stdint.h>

//Linux headers
#ifdef __linux
	#ifndef USE_SBRK
		#include <sys/mman.h>
	#endif

	#include <unistd.h>

	//pthread
	#if defined(USE_LOCK) && !defined(USE_LOCK_SPIN)
		#include<pthread.h>
		#include <stdlib.h>
	#endif //pthread
#endif //__linux

//Windows 32 headers
#ifdef _WIN32
#endif //_WIN32

/* ------------------- Typedef & Structures ------------------ */

//define Lock
#if defined(USE_LOCK) && defined(USE_LOCK_SPIN)
	typedef volatile bool lock_t;
#elif defined(USE_LOCK)
	#ifdef __linux
		typedef pthread_mutex_t lock_t;
	#endif //__linux

	#ifdef _WIN32
		//TODO: Add windows lock def
	#endif //_WIN32
#endif //USE_LOCK_SPIN

//Memory block used or small free
struct block
{
	size_t size;
	struct block * block_prev;
	struct block * block_next;
} __attribute__((packed));

//Free Memory block
struct block_free
{
	size_t size;
	struct block * block_prev;
	struct block * block_next;
	struct block_free * pool_prev;
	struct block_free * pool_next;
} __attribute__((packed));

//Holds free blocks as linked list
struct pool
{
	struct block_free * start;
	struct block_free * end;
	size_t size;
} __attribute__((packed));

/* ----------------- vars & macros ---------------- */

//Hash table
struct pool table[TABLE_SIZE];

unsigned int pool_min_index = 0;
unsigned int pool_max_index = 0;

//Pointer to last block (for sbrk and tracking)
struct block * block_last;

//Lock
#ifndef USE_LOCK_GLOBAL
	lock_t l;
#endif

#define PAGE_FAIL NULL

//Size
#define SIZE_GET(s) (s & (SIZE_MAX / 2))
#define SIZE_SET(s, x) (s = ((size_t)x | (s & ~(sizeof(size_t) / 2))))
#define SIZE_IS_USED(s) ((int)((s >> ((sizeof(size_t) * 8) - 1)) & 1))
#define SIZE_STATE_SET(s, x) (s ^= (-(size_t)x ^ s) & ((size_t)1 << ((sizeof(size_t) * 8) - 1)))

/* ------------------------------------------------- */

/*
 * @function lock_create
 * Creates lock
 *
 * @param lock_t * l
 */
void lock_create(lock_t * l)
{
	#ifdef USE_LOCK_SPIN
		l = 0;
	#endif //USE_LOCK_SPIN

	//Linux
	#if defined(__linux) && !defined(USE_LOCK_SPIN)
		pthread_mutex_init(l, NULL);
	#endif //__linux

	//Windows
	#if defined(_WIN32) && !defined(USE_LOCK_SPIN)
		//TODO: add windows lock create
	#endif

	//Add you custom lock here
}

/*
 * @function lock_remove
 * Removes lock
 *
 * @param lock_t * l
 */
void lock_remove(lock_t * l)
{
	//Linux
	#if defined(__linux) && !defined(USE_LOCK_SPIN)
		pthread_mutex_destroy(l);
	#endif //__linux

	//Windows
	#if defined(_WIN32) && !defined(USE_LOCK_SPIN)
		//TODO: add windows lock remove
	#endif

	//Add you custom lock here
}

/*
 * @function lock_wait
 * Waits for lock to be open then locks it.
 *
 * @param lock_t * l
 */
void lock_wait(lock_t * l)
{
	//Spinlock
	#ifdef USE_LOCK_SPIN
		while(!__sync_lock_test_and_set (l, false, true));
		__sync_synchronize();
	#endif //USE_LOCK_SPIN

	//Linux
	#if defined(__linux) && !defined(USE_LOCK_SPIN)
		pthread_mutex_lock(l);
	#endif //__linux

	//Windows
	#if defined(_WIN32) && !defined(USE_LOCK_SPIN)
		//TODO: add windows lock wait
	#endif

	//Add you custom lock here
}

/*
 * @function lock_signal
 * Unlocks lock and signals all waiting threads
 *
 * @param lock_t * l
 */
void lock_signal(lock_t * l)
{
	//Spinlock
	#ifdef USE_LOCK_SPIN
		__sync_synchronize();
		*l = 0;
	#endif //USE_LOCK_SPIN

	//Linux
	#if defined(__linux) && !defined(USE_LOCK_SPIN)
		pthread_mutex_unlock(l);
	#endif //__linux

	//Windows
	#if defined(_WIN32) && !defined(USE_LOCK_SPIN)
		//TODO: Add windows lock signal
	#endif //_WIN32

	//Add you custom lock here
}

/*
 * @function table_index_get
 * Takes allocation size and return the table index.
 *
 * @param size_t size
 * @return unsigned int index
 */
unsigned int table_index_get(size_t size)
{
	if (size > TABLE_SIZE)
		return (unsigned int)TABLE_SIZE;

	if (size == 0)
		return 0;

	return (unsigned int)size - 1;
}

/*
 * @function page_size_get
 * Returns the size of a page
 *
 * @return size_t pagesize
 */
size_t page_size_get(void)
{
	#ifdef __linux
		return (size_t)getpagesize();
	#elif _WIN32
		//TODO: add windows getpagesize support
	#endif

	//Guess
	return (size_t)PAGESIZE_DEFAULT;
}

/*
 * @function page_get
 * Gets a number of contiguous pages from the os. All bytes are set to 0;
 *
 * @param size_t size
 * @return void * addr to pages, PAGE_FAIL if fail
 */
void * page_get(size_t size)
{
	#ifdef USE_SBRK
		void * addr = sbrk((intptr_t)size);
		if (addr == (void *)-1)
			return NULL;

		return addr;
	#else
		if (size < page_size_get())
			size = page_size_get();

		#ifdef __linux
			//To add performance on embedded devices use MAP_UNINITIALIZED flag and set bytes to

			#ifdef MAP_ANONYMOUS
				void * addr =  mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
			#else //For systems with no MAP_ANONYMOUS eg BSD
				int fd = open("/dev/zero", O_RDWR);
				void * addr =  mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, -1, 0);
			#endif
			if(addr == MAP_FAILED)
				return PAGE_FAIL;
			else
				return addr;
		#elif _WIN32
			//TODO: add windows memory support
		#endif
	#endif

	return PAGE_FAIL; //Cannot find a function
}

/*
 * @function page_free
 * Return pages to system
 *
 * @param void * address, size_t size
 * @return int 0 success and -1 on fail
 */
int page_free(void * addr, size_t size)
{
	#ifdef USE_SBRK
		if (sbrk(-(intptr_t)(sbrk(0) - size)) == (void *)-1)
			return 1;
		return 0;
	#else
		#ifdef __linux
			return munmap(addr, size);
		#elif _WIN32
			//TODO: add windows support for free page
		#endif
	#endif
}

/*
 * @function pool_swap
 * Swaps two free nodes in pool
 *
 * @param struct block_free * a, struct block_free * b
 */
void pool_swap(struct block_free * a, struct block_free * b)
{
	if (a == NULL || b == NULL)
		return;

	//Switch prev pointers
	if (a->pool_prev != NULL)
		a->pool_prev->pool_next = b;

	struct block_free * temp = a->pool_prev;
	a->pool_prev = b;
	b->pool_prev = temp;

	//Switch next pointers
	if (b->pool_next != NULL)
		b->pool_next->pool_prev = a;

	temp = b->pool_next;
	b->pool_next = a;
	a->pool_next = temp;
}

/*
 * @function pool_sort
 * Sorts node in pool
 *
 * @param struct block_free * b
 */
void pool_sort(struct block_free * b)
{
	struct pool * p = &table[table_index_get(SIZE_GET(b->size))];

	while (b->pool_next != NULL)
	{
		if (SIZE_GET(b->size) <= SIZE_GET(b->pool_next->size))
			break;

		if (b == p->start)
			p->start = b->pool_next;

		if (b->pool_next == p->end)
			p->end = b;

		pool_swap(b, b->pool_next);
	}
}

/*
 * @function pool_insert
 * Adds free block into pool's linked list
 *
 * @param struct block_free * b
 * @return int 0 success
 */
int pool_insert(struct block_free * b)
{
	if (b == NULL)
		return -1;

	struct pool * p = &table[table_index_get(SIZE_GET(b->size))];

	//Lock pool
	lock_wait(&l);

	//First node
	if (p->start == NULL)
	{
		b->pool_prev = NULL;
		b->pool_next = NULL;
		p->start = b;
		p->end = b;
		p->size++;
		return 0;
	}

	b->pool_prev = NULL;
	b->pool_next = p->start;
	p->start->pool_prev = b;
	p->start = b;
	p->size++;
	pool_sort(b); //Sort

	if (p->size <= table[pool_min_index].size)
		pool_min_index = table_index_get(SIZE_GET(b->size));

	if (p->size >= table[pool_max_index].size)
		pool_max_index = table_index_get(SIZE_GET(b->size));

	lock_signal(&l); //Unlock
	return 0;
}

/*
 * @function pool_remove
 * Removes node from pool
 *
 * @param struct block_free * b
 * return int 0 success
 */
int pool_remove(struct block_free * b)
{
	if (b == NULL)
		return -1;

	struct pool * p = &table[table_index_get(SIZE_GET(b->size))];
	if (b == p->start)
		p->start = b->pool_next;

	if (b == p->end)
		p->end = b->pool_prev;

	if (b->pool_prev != NULL)
		b->pool_prev->pool_next = b->pool_next;

	if (b->pool_next != NULL)
		b->pool_next->pool_prev = b->pool_prev;

	b->pool_prev = NULL;
	b->pool_next = NULL;

	p->size--;

	if (p->size <= table[pool_min_index].size)
		pool_min_index = table_index_get(SIZE_GET(b->size));

	if (p->size >= table[pool_max_index].size)
		pool_max_index = table_index_get(SIZE_GET(b->size));

	return 0;
}

/*
 * @function pool_search
 * Finds free block >= size
 *
 * @param size_t s
 * @return struct block_free *
 */
struct block_free * pool_search(size_t s, struct pool * p)
{
	if (s == 0)
		return NULL;

	if (p == NULL)
		p = &table[table_index_get(s)];

	struct block_free * n = p->start;
	while (n != NULL)
		if (SIZE_GET(n->size) >= s)
			return n;
		else
			n = n->pool_next;

	return NULL;
}

/*
 * @function block_create
 * Creates block >= size. Memory can be retrieved from mmap or sbrk call.
 *
 * @param size_t size
 * @return struct block_free *
 */
struct block * block_create(size_t size)
{
	//Min allocation size if not using sbrk
	#ifndef USE_SBRK
		if (size < PAGE_MIN_ALLOC * page_size_get())
			size += PAGE_MIN_ALLOC * page_size_get();
	#endif

	//Create new block
	size += sizeof(struct block); //Add header size
	struct block * b = (struct block *)page_get(size);

	//Check alloc worked
	if (b == NULL)
		return NULL;

	#ifdef USE_SBRK
		b->block_prev = block_last;
		block_last = b;
		block_last->block_next = b;
	#else
		b->block_prev = NULL;
	#endif

	b->block_next = NULL;
	SIZE_SET(b->size, size);
	SIZE_STATE_SET(b->size, 1);
	return b;
}

/*
 * @function block_remove
 * Removes block and returns it to the system.
 *
 * @param struct block_free * b
 * @return int 0 success, 1 if block is not full width allocation, -1 on fail, 2 on fail try
 */
int block_remove(struct block_free * b)
{
	#ifdef USE_SBRK
		if ((struct block *)b != block_last)
			return -1;

		block_last = b->block_prev;

		if (block_last != NULL)
			block_last->block_next = NULL;

	#else
		if (b->block_prev != NULL && b->block_next != NULL)
			return 2;
	#endif

	return page_free((void *)b, SIZE_GET(b->size) + sizeof(struct block_free));
}

/*
 * @function block_split
 * Splits free block into a block = to size and a free block. Free block is added to a pool.
 *
 * @param size_t size, struct block_free * b (block not in pool)
 * @return struct block *
 */
struct block * block_split(size_t size, struct block_free * b)
{
	if (size == 0 || b == NULL)
		return NULL;

	//Check if size if < b->size and that a free block will fit
	if (size >= b->size || size + sizeof(struct block_free) + 1 > SIZE_GET(b->size))
		return NULL;

	struct block * n = (struct block *)b;

	b = (struct block_free *)((size_t)b + sizeof(struct block) + size);
	SIZE_SET(b->size, SIZE_GET(n->size) - (size + sizeof(struct block_free)));
	SIZE_STATE_SET(b->size, 0);
	b->block_next = NULL;
	b->block_prev = n;
	b->block_next = n->block_next;
	if (pool_insert(b) == -1)
		return NULL;

	n->block_next = (struct block *)b;
	SIZE_SET(n->size, size);
	SIZE_STATE_SET(n->size, 1);
	return n;
}

/*
 * @function block_join
 * Joins with right and remove right from pool,
 * and join with left block but will not remove b from pool.
 *
 * @param struct block_free * b
 * @returns int 0 success, -1 fail
 */
int block_join(struct block_free * b)
{
	if (b == NULL || SIZE_IS_USED(b->size))
		return -1;

	//Join with right
	if (b->block_next != NULL && !SIZE_IS_USED(b->block_next->size))
	{
		pool_remove((struct block_free *)b->block_next);
		SIZE_SET(b->size, SIZE_GET(b->block_next->size) + sizeof(struct block_free));
		b->block_next = b->block_next->block_next;
	}

	//Join with left
	if (b->block_prev != NULL && !SIZE_IS_USED(b->block_prev->size))
	{
		pool_remove(b);
		SIZE_SET(b->block_prev->size, SIZE_GET(b->size) + sizeof(struct block_free));
		b->block_prev->block_next = b->block_next;
	}

	return 0;
}

/*
 * @function mem_init
 * Sets up memory allocator pools.
 */
void mem_init(void)
{
	//Setup mutex lock statically
	#if defined(__linux) && !defined(USE_LOCK_SPIN)
		static lock_t l = PTHREAD_MUTEX_INITIALIZER;
	#else
		static lock_t l = 0;
	#endif

	lock_wait(&l);
	
	static bool complete = false;
	if (complete == true)
	{
		lock_signal(&l);
		return;
	}

	pool_min_index = 0;
	pool_max_index = 0;

	memset(table, 0, sizeof(table));
	block_last = NULL;

	complete = true;
	lock_signal(&l);
}

/*
 * @function mem_alloc
 * Get block of memory >= size. (internal malloc)
 *
 * @param size_t size
 * @return void * address
 */
void * mem_alloc(size_t size)
{
	if (size == 0)
		return NULL;

	//Setup allocator if needed.
	mem_init();

	//Get index of pool that could contain free block
	unsigned int index = table_index_get(size);
	struct block_free * b = pool_search(size, NULL);

	if (b == NULL)
		b = pool_search(size, &table[pool_max_index]);

	//If no block was found the create new one.
	if (b == NULL)
	{
		struct block * n = block_create(size);
		return (n == NULL)? NULL : (void *)n + sizeof(struct block);
	}

	//If block is perfect size return it.
	if (size + sizeof(struct block_free) + 1 > SIZE_GET(b->size))
	{
		SIZE_STATE_SET(b->size, 1);
		return (void *)b + sizeof(struct block);
	}

	//Split block and return.
	struct block * n = block_split(size, b);
	return (n == NULL)? NULL : (void *)n + sizeof(struct block);
}

/*
 * @function mem_free
 * Frees allocated memory
 *
 * @param void * address
 */
void mem_free(void * address)
{
	if (address == NULL)
		return;

	struct block_free * b = (struct block_free *)(address - sizeof(struct block));

	if (SIZE_IS_USED(b->size) == 0)
		return; //Error address is not a used block

	//Try to remove block if using mmap
	#ifndef USE_SBRK
		if (block_remove(b))
			return;
	#endif

	SIZE_STATE_SET(b->size, 0);
	if ((struct block *)b == block_last)
	{
		block_remove(b);
		return;
	}

	//Join block
	block_join(b);
	pool_insert(b);
}

#ifdef DEBUG

/*
 * Converts timespec to double
 *
 * @param struct timespec time
 * @return double time
 */
double time_to_double(struct timespec time)
{
	//1000000000.0 = conversion rate 1000 X 1000 X 1000 (sec->millsec->usec->nsec)
	return ((double) time.tv_sec + (time.tv_nsec / 1000000000.0));
}

/*
 * time_record
 *
 * @return double difference.
 */
double time_record(void)
{
	static struct timespec a;
	struct timespec b;

	if (timespec_get(&b, TIME_UTC) == 0) {
		printf("ERROR: Could not record resource usage.\n");
		exit(EXIT_FAILURE);
	}

	return time_to_double(b) - time_to_double(a);
}

int main()
{
	srand(123456);
	size_t start = (size_t)sbrk(0);
	clock_t time_start = clock();

	for (int i = 0; i < 1000000; ++i)
	{
		if (i % 10000)
			fflush(stdout);

		size_t s = (size_t)(rand() % 32) + 1;
		void * m = mem_alloc(s);
		printf("%u | %p - %zu Bytes\n", i, m, s);
		mem_free(m);
	}

	clock_t time_end = clock();
	size_t end = (size_t)sbrk(0);
	printf("\nBRK Start: %zu\n", start);
	printf("BRK End: %zu\n", end);
	printf("Difference: %zu\n", end - start);
	printf("Time taken: %e\n", (double)(time_end - time_start) / CLOCKS_PER_SEC);
	return 0;
}
#endif