#ifndef OWM_SETTINGS_H
#define OWM_SETTINGS_H

#include <stddef.h>
#include <stdio.h>

/* Initialize to zero. Owns its line buffer. The caller owns input streams. */
struct settings_reader {
	char		*buffer;
	char		*text;
	size_t		capacity;
	size_t		max_length;
	unsigned int	line_number;
};

/*
 * Returns 1 for a nonempty, noncomment line, 0 at EOF, or negative one on
 * error. Text is borrowed until the next read or settings_reader_free.
 * A zero max_length leaves line length unrestricted.
 */
int	settings_read_line(struct settings_reader *, FILE *);
/* Release the buffer and reset all fields. Safe to call again or reuse. */
void	settings_reader_free(struct settings_reader *);
int	settings_split_assignment(char *, char **, char **);

char	*trim(char *);
char	*decode_string(const char *);
void	 write_string(FILE *, const char *);
int	 parse_boolean(const char *, int *);
int	 parse_integer(const char *, int, int, int *);

#endif
