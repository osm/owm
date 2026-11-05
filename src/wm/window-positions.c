#include <stdlib.h>
#include <string.h>

#include "constraints.h"
#include "memory.h"
#include "settings.h"
#include "window-positions.h"

static int	position_field(struct window_position *, char *, char *);

void
window_positions_free(struct window_position **head)
{
	struct window_position *p = NULL;

	while (*head != NULL) {
		p = *head;
		*head = p->next;
		free(p->class_name);
		free(p->instance);
		free(p);
	}
}

static int
position_field(struct window_position *p, char *key, char *value)
{
	char **field = NULL, *decoded = NULL;
	int bit;

	if (strcmp(key, "x") == 0)
		return parse_integer(value, -WM_GEOMETRY_LIMIT,
			WM_GEOMETRY_LIMIT, &p->x)
			? 1
			: 0;
	if (strcmp(key, "y") == 0)
		return parse_integer(value, -WM_GEOMETRY_LIMIT,
			WM_GEOMETRY_LIMIT, &p->y)
			? 2
			: 0;
	if (strcmp(key, "class") == 0) {
		field = &p->class_name;
		bit = 4;
	} else if (strcmp(key, "instance") == 0) {
		field = &p->instance;
		bit = 8;
	} else
		return 0;
	decoded = decode_string(value);
	if (decoded == NULL)
		return 0;
	if (strlen(decoded) > MAX_TEXT) {
		free(decoded);
		return 0;
	}
	free(*field);
	*field = decoded;

	return bit;
}

int
window_positions_read(FILE *file, struct window_position **head)
{
	struct settings_reader reader = {0};
	struct window_position *current = NULL, **tail = NULL;
	int status, fields, bit, count;

	reader.max_length = MAX_TEXT * 4;
	tail = head;
	fields = count = 0;
	while ((status = settings_read_line(&reader, file)) > 0) {
		char *key = NULL, *value = NULL;

		if (strcmp(reader.text, "position {") == 0) {
			if (current != NULL || ++count > 4096)
				break;
			current = allocate(sizeof(*current));
			fields = 0;
			continue;
		}
		if (strcmp(reader.text, "}") == 0) {
			if (current == NULL || fields != 15 ||
				(*current->class_name == '\0' &&
					*current->instance == '\0'))
				break;
			*tail = current;
			tail = &current->next;
			current = NULL;
			continue;
		}
		if (current == NULL ||
			!settings_split_assignment(reader.text, &key, &value))
			break;
		bit = position_field(current, key, value);
		if (bit == 0 || (fields & bit))
			break;
		fields |= bit;
	}
	settings_reader_free(&reader);
	if (status != 0 || current != NULL) {
		window_positions_free(&current);
		window_positions_free(head);
		return 0;
	}

	return 1;
}

int
window_positions_write(FILE *file, const struct window_position *head)
{
	const struct window_position *p = NULL;

	fputs("# owm remembered window positions: frame x/y.\n", file);
	for (p = head; p != NULL; p = p->next) {
		fputs("position {\n    class = ", file);
		write_string(file, p->class_name);
		fputs("\n    instance = ", file);
		write_string(file, p->instance);
		fprintf(file, "\n    x = %d\n    y = %d\n}\n", p->x, p->y);
	}

	return !ferror(file);
}
