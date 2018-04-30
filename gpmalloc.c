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

//Memory block
struct block
{
	size_t size;
	struct block * prev;
	struct block * next;
} __attribute__((packed));

struct node
{
	struct block * parent; //Parent node
	struct block * a; //Child A
	struct block * b; //Child B
} __attribute__((packed));

//Alloction tree
struct tree
{
	struct block * root; //Address to root
	struct block * last; //right most leaf
	size_t size; //Number of nodes in tree
} __attribute__((packed));

/* ----------------- vars & macros ---------------- */

//Hash table
struct tree table[TABLE_SIZE];

#define PAGE_FAIL NULL
#define HEADER_SIZE (sizeof(struct block) + sizeof(struct node))
#define BLOCK_NODE_GET(b) ((struct node *)b + sizeof(struct block))

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
 * @function tree_swap
 * Swaps two tree nodes.
 *
 * @param struct node * A, struct node * B
 */
void tree_swap(struct node * a, struct node * b)
{
	struct node temp = *a;
	*a = *b;
	*b = temp;
}

/*
 * @function tree_compare
 * Compares nodes and returns true if node a is greater
 *
 * @param struct block * a, struct block * b
 * @return bool
 */
bool tree_compare(struct block * a, struct block * b)
{
	if (a->size > b->size || a->size == b->size && a > b)
		return true;

	return false;
}

/*
 * @function tree_insert
 * Insert free node into tree
 *
 * @param struct block * b, struct tree * t
 * @return int 0 if success
 */
int tree_insert(struct block * b, struct tree * t)
{
	if (b == NULL || t == NULL)
		return -1;

	//Set children to NULL
	struct node * n = BLOCK_NODE_GET(b);
	n->a = NULL;
	n->b = NULL;

	//If tree is empty then insert at root
	if (t->root == NULL)
	{
		t->root = b;
		t->last = b;
		t->size++;
		return 0;
	}

	//Find insert point
	struct block * i = t->last;
	while(BLOCK_NODE_GET(i)->parent != NULL && i == BLOCK_NODE_GET(BLOCK_NODE_GET(i)->parent)->b)
		i = BLOCK_NODE_GET(i)->parent;

	if (BLOCK_NODE_GET(i)->parent != NULL)
	{
		if (BLOCK_NODE_GET(BLOCK_NODE_GET(i)->parent)->b != NULL)
		{
			i = BLOCK_NODE_GET(BLOCK_NODE_GET(i)->parent)->b;
			while (BLOCK_NODE_GET(i)->b != NULL)
				i = BLOCK_NODE_GET(i)->b;
		}
		else
			i = BLOCK_NODE_GET(i)->parent;
	}
	else
		while (BLOCK_NODE_GET(i)->a != NULL)
			i = BLOCK_NODE_GET(i)->a;

	//Insert node
	if (BLOCK_NODE_GET(i)->a == NULL)
		BLOCK_NODE_GET(i)->a = b;
	else
		BLOCK_NODE_GET(i)->b = b;

	BLOCK_NODE_GET(b)->parent = i;
	BLOCK_NODE_GET(b)->a = NULL;
	BLOCK_NODE_GET(b)->b = NULL;
	t->last = b;
	t->size++;

	//Fix heap
	i = b;
	while (BLOCK_NODE_GET(i)->parent != NULL && tree_compare(BLOCK_NODE_GET(i)->parent, i))
	{
		//Swap with parent and move up
		tree_swap(BLOCK_NODE_GET(i), BLOCK_NODE_GET(BLOCK_NODE_GET(i)->parent));
		i = BLOCK_NODE_GET(i)->parent;
	}

	return 0;
}

//TODO: add remove node function
//TODO: add search node function

#ifdef DEBUG
int main()
{

	char array[255];
	struct block * b = (struct block *)array;
	tree_insert(b, &table[0]);
	struct tree * t = &table[0];
	printf("t: %p\nSize: %zu\nRoot: %p\nLast: %p\n", t, t->size, t->root, t->last);
	return 0;
}
#endif