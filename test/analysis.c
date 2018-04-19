/* General Purpose Memory Allocator (gpmalloc)
 * analysis.c
 *
 * Copyright (C) 2018
 * All Rights Reserved
 */

#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <zconf.h>

#if defined(__linux) || defined(__unix)
	#include <sys/resource.h>
	#include <memory.h>
	#include <errno.h>
#endif

//Options base
#define STEPS 20000
#define POINTER_NUMBER 1024
#define SEED 1234
#define UNCERTAINTY_STEPS 1000

//Options size
#define SIZE_ALLOC_MAX 4096
#define SIZE_ALLOC_MIN 1
//#define SIZE_ALLOC_FIXED

//Options functions
#define __malloc malloc
#define __free free

//Options results
#define FILE_DUMP


struct pointer
{
	void * addr;
	size_t size;
};

struct pointer pointers[POINTER_NUMBER];

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
 * Calculates uncertainty of timespec function
 *
 * @return double time
 */
double uncertainty_get(void)
{
	double uncertainty = 0;
	for (int i = 0; i < UNCERTAINTY_STEPS; ++i)
	{
		struct timespec ts_start;
		if (timespec_get(&ts_start, TIME_UTC) == 0) {
			printf("ERROR: Could not record resource usage.\n");
			exit(EXIT_FAILURE);
		}

		struct timespec ts_end;
		if (timespec_get(&ts_end, TIME_UTC) == 0) {
			printf("ERROR: Could not record resource usage.\n");
			exit(EXIT_FAILURE);
		}

		uncertainty += time_to_double(ts_end) - time_to_double(ts_start);
	}

	return uncertainty / UNCERTAINTY_STEPS;
}

int main(int argc, char **argv)
{
	double time_average_malloc = 0;
	double time_average_free = 0;
	double memory_average_alloc = 0;
	unsigned int fails = 0;

	//Setup seed
	#ifndef SEED
	const unsigned int SEED = (const unsigned int)time(NULL);
	#endif

	srand(SEED);

	//Open dump file
	#ifdef FILE_DUMP
		char name[255];
		sprintf(name, "d_%d.csv", (int)time((time_t*)0));
		FILE * fp = fopen(name, "w");

		if (fp == NULL)
		{
			printf("ERROR: could not open dump file.\n");
			exit(EXIT_FAILURE);
		}

		//Setup csv
		fprintf(fp, "Iteration,Pointer Address (Dec),Allocation Size (dec),Time Taken (ns),Space Used (bytes),Status\n");
	#endif

	//Clear all pointers
	for (int i = 0; i < POINTER_NUMBER; ++i)
		pointers[i].addr = NULL;

	//Record resources for start
	#if defined(__linux) || defined(__unix)
		unsigned int memory_start = (unsigned int)sbrk(0);
	#endif

	clock_t time_start = clock();

	for (int i = 0; i < STEPS; ++i)
	{
		struct timespec ts_start, ts_end;
		//Generate numbers
		#ifdef SIZE_ALLOC_FIXED
		unsigned int size = SIZE_ALLOC_FIXED;
		#else
		size_t size = (size_t)rand() % (SIZE_ALLOC_MAX + 1 - SIZE_ALLOC_MIN) + SIZE_ALLOC_MIN;
		#endif

		unsigned int index = (unsigned int)rand() % POINTER_NUMBER;

		//Record mem
		#if defined(__linux) || defined(__unix)
		unsigned int m_start = (unsigned int)sbrk(0);
		#endif

		//Record time
		if (timespec_get(&ts_start, TIME_UTC) == 0)
		{
			printf("ERROR: Could not record resource usage.\n");
			exit(EXIT_FAILURE);
		}

		//test free
		if (pointers[index].addr == NULL)
			__free(pointers[index].addr);

		if (timespec_get(&ts_end, TIME_UTC) == 0)
		{
			printf("ERROR: Could not record resource usage.\n");
			exit(EXIT_FAILURE);
		}

		time_average_free += time_to_double(ts_end) - time_to_double(ts_start);

		//Record time
		if (timespec_get(&ts_start, TIME_UTC) == 0)
		{
			printf("ERROR: Could not record resource usage.\n");
			exit(EXIT_FAILURE);
		}

		//test malloc
		pointers[index].addr = __malloc(size);

		if (timespec_get(&ts_end, TIME_UTC) == 0)
		{
			printf("ERROR: Could not record resource usage.\n");
			exit(EXIT_FAILURE);
		}


		#if defined(__linux) || defined(__unix)
		unsigned int m_end = (unsigned int)sbrk(0);
		#endif

		if (pointers[index].addr == NULL)
			fails++;

		//Record size
		pointers[index].size = size;

		time_average_malloc += time_to_double(ts_end) - time_to_double(ts_start);
		memory_average_alloc += size;

		//Record data to dump file
		#ifdef FILE_DUMP
			fprintf(fp, "%d,%llu,%d,%e,%d,%s\n",
			        i + 1,
			        (long long int)pointers[index].addr,
					(int)size,
					time_to_double(ts_end) - time_to_double(ts_start),
					(int)m_end - (int)m_start,
					(pointers[index].addr == NULL)?"Fail":"Success");
		#endif
	}

	//Close dump file
	#ifdef FILE_DUMP
		fclose(fp);
	#endif

	//Record resources for end
	clock_t time_end = clock();

	#if defined(__linux) || defined(__unix)
	unsigned int memory_end = (unsigned int)sbrk(0);
	#endif

	//Print results
	printf("Results\n+");
	for (int i = 0; i < 46; ++i)
		putchar('-');
	printf("+\n");

	printf("| %-25s | %16d |\n", "Steps", (int)STEPS);
	printf("| %-25s | %16d |\n", "Number of pointers", (int)POINTER_NUMBER);
	printf("| %-25s | %16d |\n", "SEED", (int)SEED);
	#ifdef FILE_DUMP
		printf("| %-25s | %16s |\n", "Dump file", name);
	#endif
	printf("| %-25s | %16d |\n", "Minimum allocation size", (int)SIZE_ALLOC_MIN);
	printf("| %-25s | %16d |\n", "Maximum allocation size", (int)SIZE_ALLOC_MAX);

	#ifdef SIZE_ALLOC_FIXED
	printf("| %-25s | %16d |\n", "Fixed allocation size", (int)SIZE_ALLOC_FIXED);
	#endif

	printf("| %-25s | %16d |\n", "Fails", (int)fails);

	//Time taken
	printf("|");
	for (int i = 0; i < 46; ++i)
		putchar('-');
	printf("|\n");

	printf("| %-25s | %16e |\n", "CPU time used", (double)(time_end - time_start) / CLOCKS_PER_SEC);
	printf("| %-25s | %8d/1000000 |\n", "CLOCKS_PER_SEC", (int)CLOCKS_PER_SEC);
	printf("| %-25s | %16e |\n", "Average malloc time", time_average_malloc / STEPS);
	printf("| %-25s | %16e |\n", "Average free time", time_average_free / STEPS);
	printf("| %-25s | %16e |\n", "Uncertainty (+-)", uncertainty_get() / STEPS);
	printf("| %-25s | %16d |\n", "Uncertainty steps", UNCERTAINTY_STEPS);

	//Memory used
	printf("|");
	for (int i = 0; i < 46; ++i)
		putchar('-');
	printf("|\n");

	#if defined(__linux) || defined(__unix)
	printf("| %-25s | %16d |\n", "Memory used (sbrk)", memory_end - memory_start);
	#endif

	printf("| %-25s | %16lf |\n", "Average memory allocated", memory_average_alloc / STEPS);

	putchar('+');
	for (int i = 0; i < 46; ++i)
		putchar('-');
	printf("+\n");

	return EXIT_SUCCESS;
}