#include <err.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "constraints.h"
#include "memory.h"
#include "session-file.h"
#include "settings.h"

#define MAX_SESSION_RECORDS 4096

static void	free_record(struct session_record *);
static int	string_field(char **, const char *);
static int	session_field(struct session_record *, const char *, char *,
	unsigned int *);
static void	save_text(FILE *, const char *, const char *);
static void	write_record(FILE *, const struct session_record *);

enum {
	FIELD_X = 1,
	FIELD_Y = 2,
	FIELD_WIDTH = 4,
	FIELD_HEIGHT = 8,
	GEOMETRY_FIELDS = FIELD_X | FIELD_Y | FIELD_WIDTH | FIELD_HEIGHT
};

static void
free_record(struct session_record *r)
{
	free(r->class_name);
	free(r->instance);
	free(r->title);
	free(r->command);
	free(r);
}

void
session_records_free(struct session_record **head)
{
	while (*head != NULL) {
		struct session_record *next = NULL;

		next = (*head)->next;
		free_record(*head);
		*head = next;
	}
}

static int
string_field(char **field, const char *value)
{
	char *decoded = NULL;

	decoded = decode_string(value);

	if (decoded == NULL)
		return 0;

	if (strlen(decoded) > MAX_TEXT) {
		free(decoded);
		return 0;
	}

	free(*field);
	*field = decoded;

	return 1;
}

static int
session_field(struct session_record *current, const char *key, char *value,
	unsigned int *fields)
{
	int ok;

	if (strcmp(key, "class") == 0)
		return string_field(&current->class_name, value);

	if (strcmp(key, "instance") == 0)
		return string_field(&current->instance, value);

	if (strcmp(key, "title") == 0)
		return string_field(&current->title, value);

	if (strcmp(key, "command") == 0)
		return string_field(&current->command, value);

	if (strcmp(key, "x") == 0) {
		ok = parse_integer(value, -WM_GEOMETRY_LIMIT, WM_GEOMETRY_LIMIT,
			&current->g.x);
		*fields |= FIELD_X;
		return ok;
	}

	if (strcmp(key, "y") == 0) {
		ok = parse_integer(value, -WM_GEOMETRY_LIMIT, WM_GEOMETRY_LIMIT,
			&current->g.y);
		*fields |= FIELD_Y;
		return ok;
	}

	if (strcmp(key, "width") == 0) {
		ok = parse_integer(value, 1, WM_GEOMETRY_LIMIT, &current->g.w);
		*fields |= FIELD_WIDTH;
		return ok;
	}

	if (strcmp(key, "height") == 0) {
		ok = parse_integer(value, 1, WM_GEOMETRY_LIMIT, &current->g.h);
		*fields |= FIELD_HEIGHT;
		return ok;
	}

	if (strcmp(key, "order") == 0)
		return parse_integer(value, 0, INT_MAX - 2, &current->order);

	if (strcmp(key, "desktop") == 0) {
		ok = parse_integer(value, 0, MAX_DESKTOPS, &current->desktop);
		current->desktop--;
		return ok;
	}

	if (strcmp(key, "minimized") == 0)
		return parse_boolean(value, &current->minimized);

	if (strcmp(key, "fullscreen") == 0)
		return parse_boolean(value, &current->fullscreen);

	if (strcmp(key, "maximized_horizontal") == 0)
		return parse_boolean(value, &current->max_h);

	if (strcmp(key, "maximized_vertical") == 0)
		return parse_boolean(value, &current->max_v);

	return 0;
}

int
session_file_read(FILE *file, const char *path, int *saved_desktop,
	struct session_record **head)
{
	struct session_record *current = NULL;
	struct settings_reader reader = {0};
	int ok, status;
	unsigned int count, fields;

	reader.max_length = MAX_TEXT * 4;

	fields = 0;
	count = 0;

	ok = 1;

	while ((status = settings_read_line(&reader, file)) > 0) {
		char *key = NULL, *value = NULL;

		key = reader.text;

		if (current == NULL &&
			strncmp(key, "current_desktop", 15) == 0) {
			ok = settings_split_assignment(
				reader.text, &key, &value) &&
				strcmp(key, "current_desktop") == 0 &&
				parse_integer(
					value, 1, MAX_DESKTOPS, saved_desktop);

			if (!ok)
				break;

			continue;
		}

		if (strcmp(key, "window {") == 0) {
			if (current != NULL || ++count > MAX_SESSION_RECORDS) {
				ok = 0;
				break;
			}

			current = allocate(sizeof(*current));
			current->class_name = copy_string("");
			current->instance = copy_string("");
			current->title = copy_string("");
			current->command = copy_string("");
			current->order = (int)count - 1;
			fields = 0;
			continue;
		}

		if (strcmp(key, "}") == 0) {
			struct session_record **p = NULL;

			if (current == NULL || fields != GEOMETRY_FIELDS ||
				(*current->class_name == '\0' &&
					*current->instance == '\0')) {
				ok = 0;
				break;
			}

			p = head;

			while (*p != NULL && (*p)->order <= current->order)
				p = &(*p)->next;
			current->next = *p;
			*p = current;
			current = NULL;
			continue;
		}

		if (current == NULL ||
			!settings_split_assignment(reader.text, &key, &value)) {
			ok = 0;
			break;
		}

		ok = session_field(current, key, value, &fields);

		if (!ok)
			break;
	}

	if (status < 0 || current != NULL)
		ok = 0;
	if (!ok)
		warnx("%s:%u: invalid session", path, reader.line_number);
	settings_reader_free(&reader);

	if (current != NULL)
		free_record(current);

	if (!ok) {
		session_records_free(head);
		return 0;
	}

	return 1;
}

static void
save_text(FILE *file, const char *key, const char *value)
{
	fprintf(file, "    %s = ", key);
	write_string(file, value);
	fputc('\n', file);
}

static void
write_record(FILE *file, const struct session_record *record)
{
	fputs("window {\n", file);
	save_text(file, "class", record->class_name);
	save_text(file, "instance", record->instance);
	save_text(file, "title", record->title);
	save_text(file, "command",
		(record->command != NULL) ? record->command : "");
	fprintf(file,
		"    x = %d\n    y = %d\n    width = %d\n    height = "
		"%d\n"
		"    minimized = %s\n    "
		"maximized_horizontal = %s\n"
		"    maximized_vertical = %s\n    fullscreen = %s\n    "
		"desktop = "
		"%d\n    order = %d\n}\n",
		record->g.x, record->g.y, record->g.w, record->g.h,
		record->minimized ? "true" : "false",
		record->max_h ? "true" : "false",
		record->max_v ? "true" : "false",
		record->fullscreen ? "true" : "false", record->desktop + 1,
		record->order);
}

int
session_file_write(
	FILE *file, int desktop, const struct session_record *records)
{
	const struct session_record *r = NULL;

	fputs("# owm session v1: frame x/y, client pixel width/height.\n# "
	"command is a shell command; captured "
	"WM_COMMAND arguments are individually quoted.\n",
		file);
	fprintf(file, "current_desktop = %d\n", desktop);
	for (r = records; r != NULL; r = r->next)
		write_record(file, r);

	return !ferror(file);
}
