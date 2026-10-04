#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "string-util.h"

void *
allocate(size_t size)
{
	return allocate_array(1, size);
}

void *
allocate_array(size_t count, size_t size)
{
	void *p = NULL;

	p = calloc(count, size);

	if (p == NULL)
		errx(1, "out of memory");

	return p;
}

char *
copy_string(const char *s)
{
	size_t size;
	char *p = NULL;

	if (s == NULL)
		s = "";

	size = strlen(s) + 1;
	p = allocate(size);

	string_copy(p, s, size);

	return p;
}
