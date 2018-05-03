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
//#define USE_LOCK_GLOBAL
//#define USE_LOCK_SPIN

//Tree table
#define TABLE_SIZE 4096

//Defult Pagesize
#define PAGESIZE_DEFAULT 4096

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

//Linux headers
#ifdef __linux
	#include <sys/mman.h>
	#include <unistd.h>

	//pthread
	#if defined(USE_LOCK) && !defined(USE_LOCK_SPIN)
		#include<pthread.h>
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

	#ifndef USE_LOCK_GLOBAL
		lock_t l;
	#endif;
} __attribute__((packed));

/* ----------------- vars & macros ---------------- */

//Hash table
struct pool table[TABLE_SIZE];

#define PAGE_FAIL NULL

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

	return (unsigned int)size;
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
	if (size < page_size_get())
		size = page_size_get();

	//TODO: add sbrk support for small allocation on linux
	#ifdef __linux
		//To add performance on embedded devices use MAP_UNINITIALIZED flag and set bytes to

		#ifdef MAP_ANONYMOUS
			void * addr =  mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
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
	#else
		//TODO: add support for sbrk or custom heap calls
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
	#ifdef __linux
		return munmap(addr, size);
	#elif _WIN32
		//TODO: add windows support for free page
	#else
		//TODO: add support for sbrk or custom heap calls
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
 * @param struct block_free * b, struct pool * p
 */
void pool_sort(struct block_free * b, struct pool * p)
{
	while (b->pool_next != NULL)
	{
		if (b->size <= b->pool_next->size)
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
 * @param struct block_free * b, struct pool * p
 * @return int 0 success
 */
int pool_insert(struct block_free * b, struct pool * p)
{
	if (b == NULL || p == NULL)
		return -1;

	//Lock pool
	lock_wait(&p->l);

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
	pool_sort(b, p); //Sort
	lock_signal(&p->l); //Unlock
	return 0;
}

/*
 * @function pool_remove
 * Removes node from pool
 *
 * @param struct block_free * b, struct pool * p
 * return int 0 success
 */
int pool_remove(struct block_free * b, struct pool * p)
{
	if (b == NULL || p == NULL)
		return -1;

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
	return 0;
}

/*
 * @function pool_search
 * Finds free block >= size
 *
 * @param size_t s, struct pool * p
 * @return struct block_free *
 */
struct block_free * pool_search(size_t s, struct pool * p)
{
	if (s == 0 || p == NULL)
		return NULL;

	struct block_free * n = p->start;
	while (n != NULL)
		if (n->size >= s)
			return n;
		else
			n = n->pool_next;

	return NULL;
}

#ifdef DEBUG
int main()
{
	memset(table, 0, sizeof(table));

	struct block_free a;
	a.size = 100;
	pool_insert(&a, &table[0]);

	struct block_free b;
	b.size = 300;
	pool_insert(&b, &table[0]);

	struct block_free c;
	c.size = 200;
	pool_insert(&c, &table[0]);

	struct pool * p = &table[0];
	struct block_free * n = pool_search(200, &table[0]);
	printf("%p\n%zu\n", n, n->size);
}
#endif