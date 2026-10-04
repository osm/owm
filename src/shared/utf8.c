#include "utf8.h"

size_t
utf8_next(const char *text, size_t offset)
{
	if (text[offset] == '\0')
		return offset;
	offset++;
	while (((unsigned char)text[offset] & 0xc0) == 0x80)
		offset++;

	return offset;
}

size_t
utf8_previous(const char *text, size_t offset)
{
	return offset ? utf8_floor(text, offset - 1) : 0;
}

size_t
utf8_floor(const char *text, size_t offset)
{
	while (offset && ((unsigned char)text[offset] & 0xc0) == 0x80)
		offset--;

	return offset;
}
