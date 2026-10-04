#ifndef OWM_UTF8_H
#define OWM_UTF8_H

#include <stddef.h>

/*
 * Byte offsets into a NUL terminated string, no greater than strlen(text).
 * Next/previous move one code point from a boundary, stopping at either end.
 * Floor moves back to a boundary if offset points into continuation bytes.
 * These helpers navigate boundaries. They do not validate UTF8 encoding.
 */
size_t	utf8_next(const char *, size_t);
size_t	utf8_previous(const char *, size_t);
size_t	utf8_floor(const char *, size_t);

#endif
