/* General Purpose Memory Allocator (gpmalloc)
 * gpmalloc.c
 *
 * Copyright (C) 2018
 * All Rights Reserved
 */

/* -------------------- Options -------------------- */

#define USE_HEADER

//Locks
#define USE_LOCK
//#define USE_LOCK_GLOBAL
//#define USE_LOCK_SPIN

/* -------------------- Headers -------------------- */

#ifdef USE_HEADER
	#include "gpmalloc.h"
#endif

#include <stdbool.h>

//Linux headers
#ifdef __linux

	//pthread
	#if defined(USE_LOCK) && !defined(USE_LOCK_SPIN)
		#include<pthread.h>
	#endif //pthread
#endif //__linux

//Windows 32 headers
#ifdef _WIN32
#endif //_WIN32

/* ----------------- Typedef & vars ---------------- */

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

/* ------------------- Structures ------------------ */

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