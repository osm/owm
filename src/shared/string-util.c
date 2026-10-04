#include <string.h>

#include "string-util.h"

size_t
string_copy(char *destination, const char *source, size_t size)
{
	size_t copied, length;

	length = strlen(source);

	if (size == 0)
		return length;

	copied = length < size ? length : size - 1;
	memcpy(destination, source, copied);
	destination[copied] = '\0';

	return length;
}
