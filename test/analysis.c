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

//Options
#define STEPS 4098
#define SIZE_ALLOC_MAX 4096
#define SIZE_ALLOC_MIN 1
//#define SIZE_ALLOC_FIXED
#define SEED 1234
#define UNCERTAINTY_STEPS 1000

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
	double time_average = 0;
	double memory_average_extra = 0;
	//Setup seed
	#ifndef SEED
	const unsigned int SEED = (const unsigned int)time(NULL);
	#endif

	srand(SEED);

	//Record resources for start
	#if defined(__linux) || defined(__unix)
		unsigned int memory_start = (unsigned int)sbrk(0);
	#endif

	clock_t time_start = clock();

	for (int i = 0; i < STEPS; ++i)
	{
		#if defined(__linux) || defined(__unix)
		unsigned int m_start = (unsigned int)sbrk(0);
		#endif

		struct timespec ts_start;
		if (timespec_get(&ts_start, TIME_UTC) == 0)
		{
			printf("ERROR: Could not record resource usage.\n");
			exit(EXIT_FAILURE);
		}

		//Testcase core
		#ifdef SIZE_ALLOC_FIXED
		unsigned int r = SIZE_ALLOC_FIXED;
		#else
		unsigned int r = (unsigned int)rand() % (SIZE_ALLOC_MAX - SIZE_ALLOC_MIN) + SIZE_ALLOC_MAX;
		#endif

		malloc(r);

		struct timespec ts_end;
		if (timespec_get(&ts_end, TIME_UTC) == 0)
		{
			printf("ERROR: Could not record resource usage.\n");
			exit(EXIT_FAILURE);
		}

		#if defined(__linux) || defined(__unix)
		unsigned int m_end = (unsigned int)sbrk(0);
		#endif

		time_average += time_to_double(ts_end) - time_to_double(ts_start);
		memory_average_extra += (double)(m_end - m_start) - r;
	}

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
	printf("| %-25s | %16d |\n", "SEED", (int)SEED);
	printf("| %-25s | %16d |\n", "Minimum allocation size", (int)SIZE_ALLOC_MIN);
	printf("| %-25s | %16d |\n", "Maximum allocation size", (int)SIZE_ALLOC_MAX);

	#ifdef SIZE_ALLOC_FIXED
	printf("| %-25s | %16d |\n", "Fixed allocation size", (int)SIZE_ALLOC_FIXED);
	#endif

	//Time taken
	printf("|");
	for (int i = 0; i < 46; ++i)
		putchar('-');
	printf("|\n");

	printf("| %-25s | %16e |\n", "CPU time used", (double)(time_end - time_start) / CLOCKS_PER_SEC);
	printf("| %-25s | %8d/1000000 |\n", "CLOCKS_PER_SEC", (int)CLOCKS_PER_SEC);
	printf("| %-25s | %16e |\n", "Average time", time_average / STEPS);
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

	printf("| %-25s | %16lf |\n", "Average extra memory used", memory_average_extra / STEPS);

	putchar('+');
	for (int i = 0; i < 46; ++i)
		putchar('-');
	printf("+\n");

	return EXIT_SUCCESS;
}