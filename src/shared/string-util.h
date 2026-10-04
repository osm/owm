#ifndef OWM_STRING_UTIL_H
#define OWM_STRING_UTIL_H

#include <stddef.h>

/*
 * Copy into a nonoverlapping buffer, terminating it when size is nonzero.
 * Return the source length. A result >= size indicates truncation.
 * A zero size permits a NULL destination and writes nothing.
 */
size_t	string_copy(char *, const char *, size_t);

#endif
