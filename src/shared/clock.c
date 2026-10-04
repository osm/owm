#include <time.h>

#include "clock.h"

double
monotonic_time(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);

	return (double)t.tv_sec + (double)t.tv_nsec / 1000000000.0;
}
