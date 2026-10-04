#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "settings.h"

char *
trim(char *s)
{
	char *end = NULL;

	while (isspace((unsigned char)*s))
		s++;

	end = s + strlen(s);

	while (end > s && isspace((unsigned char)end[-1]))
		*--end = '\0';

	return s;
}

int
settings_read_line(struct settings_reader *reader, FILE *file)
{
	ssize_t length;

	while ((length = getline(&reader->buffer, &reader->capacity, file)) >=
		0) {
		reader->line_number++;

		if ((reader->max_length &&
			(size_t)length > reader->max_length) ||
			memchr(reader->buffer, '\0', (size_t)length) != NULL) {
			errno = EINVAL;
			return -1;
		}

		reader->text = trim(reader->buffer);
		if (*reader->text != '\0' && *reader->text != '#')
			return 1;
	}

	return ferror(file) || feof(file) == 0 ? -1 : 0;
}

void
settings_reader_free(struct settings_reader *reader)
{
	free(reader->buffer);
	memset(reader, 0, sizeof(*reader));
}

int
settings_split_assignment(char *text, char **key, char **value)
{
	char *equal = NULL;

	equal = strchr(text, '=');

	if (equal == NULL)
		return 0;

	*equal = '\0';
	*key = trim(text);
	*value = trim(equal + 1);

	return 1;
}

/*
 * The same small grammar for quoted strings is used by config and session
 * files.
 */
char *
decode_string(const char *s)
{
	char *out = NULL, *result = NULL;

	if (*s++ != '"')
		return NULL;

	result = allocate(strlen(s) + 1);
	out = result;

	while (*s != '\0' && *s != '"') {
		char c;

		c = *s++;

		if (c != '\\') {
			*out++ = c;
			continue;
		}

		c = *s++;
		switch (c) {
		case 'n':
			c = '\n';
			break;
		case 'r':
			c = '\r';
			break;
		case 't':
			c = '\t';
			break;
		case '\\':
		case '"':
			break;
		default:
			free(result);
			return NULL;
		}

		*out++ = c;
	}

	if (*s != '"') {
		free(result);
		return NULL;
	}

	s++;

	while (isspace((unsigned char)*s))
		s++;

	if (*s != '\0' && *s != '#') {
		free(result);
		return NULL;
	}

	*out = '\0';

	return result;
}

void
write_string(FILE *file, const char *s)
{
	fputc('"', file);

	for (; s != NULL && *s != '\0'; s++) {
		switch (*s) {
		case '\\':
			fputs("\\\\", file);
			break;

		case '"':
			fputs("\\\"", file);
			break;

		case '\n':
			fputs("\\n", file);
			break;

		case '\r':
			fputs("\\r", file);
			break;

		case '\t':
			fputs("\\t", file);
			break;

		default:
			fputc((unsigned char)*s, file);
			break;
		}
	}

	fputc('"', file);
}

int
parse_integer(const char *s, int low, int high, int *value)
{
	long n;
	char *end = NULL;

	errno = 0;

	n = strtol(s, &end, 10);

	if (errno || end == s || n < low || n > high)
		return 0;

	while (isspace((unsigned char)*end))
		end++;

	if (*end != '\0')
		return 0;

	*value = (int)n;

	return 1;
}

int
parse_boolean(const char *value, int *out)
{
	if (strcmp(value, "true") && strcmp(value, "false"))
		return 0;

	*out = (strcmp(value, "true") == 0);

	return 1;
}
