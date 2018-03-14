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

#ifdef __linux
	#include <sys/resource.h>
	#include <memory.h>
	#include <errno.h>
#endif

//Options
#define STEPS 1000000
#define SIZE_ALLOC_MAX 4096
#define SIZE_ALLOC_MIN 1
#define SIZE_ALLOC_FIXED
#define SEED 1234

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

/*#ifdef __linux
	struct rusage r_usage_start;
	if (getrusage(RUSAGE_SELF, &r_usage_start) == -1)
	{
		printf("ERROR: Could not record resource usage.\n");
		printf("Linux errno: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	#endif*/

int main(int argc, char **argv)
{
	double average_total = 0;
	//Setup seed
	#ifndef SEED
	const unsigned int SEED = (const unsigned int)time(NULL);
	#endif

	srand(SEED);

	//Record resources for start
	clock_t time_start = clock();

	for (int i = 0; i < STEPS; ++i)
	{
		struct timespec ts_start;
		if (timespec_get(&ts_start, TIME_UTC) == 0)
		{
			printf("ERROR: Could not record resource usage.\n");
			exit(EXIT_FAILURE);
		}

		//Testcase core
		volatile int * r = (int *)malloc(1024);

		struct timespec ts_end;
		if (timespec_get(&ts_end, TIME_UTC) == 0)
		{
			printf("ERROR: Could not record resource usage.\n");
			exit(EXIT_FAILURE);
		}

		average_total += time_to_double(ts_end) - time_to_double(ts_start);
	}

	//Record resources for end
	clock_t time_end = clock();

	//Print results
	printf("Linux Results\n+");
	for (int i = 0; i < 46; ++i)
		putchar('-');
	printf("+\n");

	//Time taken
	printf("|");
	for (int i = 0; i < 46; ++i)
		putchar('-');
	printf("|\n");
	printf("| %-25s | %16.8lf |\n", "CPU time used", (double)(time_end - time_start) / CLOCKS_PER_SEC);
	printf("| %-25s | %8d/1000000 |\n", "CLOCKS_PER_SEC", (int)CLOCKS_PER_SEC);
	printf("| %-25s | %16.14lf |\n", "Average time", average_total / STEPS);


	putchar('+');
	for (int i = 0; i < 46; ++i)
		putchar('-');
	printf("+\n");

	return EXIT_SUCCESS;
}