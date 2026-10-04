#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "paths.h"
#include "settings.h"
#include "string-util.h"
#include "ui-config.h"

int
ui_config_parse_font(char name[FONT_NAME_SIZE], const char *text)
{
	char *value = NULL;
	int ok;

	value = decode_string(text);
	ok = value != NULL && *value != '\0' && strlen(value) < FONT_NAME_SIZE;
	if (ok)
		string_copy(name, value, FONT_NAME_SIZE);
	free(value);

	return ok;
}

void
ui_config_font(char name[FONT_NAME_SIZE])
{
	FILE *file = NULL;
	struct settings_reader reader = {0};
	char directory[PATH_MAX], path[PATH_MAX];

	string_copy(name, DEFAULT_FONT, FONT_NAME_SIZE);

	if (!config_directory(directory, sizeof(directory)))
		return;

	if (snprintf(path, sizeof(path), "%s/config", directory) >=
		(int)sizeof(path))
		return;

	file = fopen(path, "r");

	if (file == NULL)
		return;

	while (settings_read_line(&reader, file) > 0) {
		char *key = NULL, *text = NULL;

		if (!settings_split_assignment(reader.text, &key, &text) ||
			strcmp(key, "font"))
			continue;

		ui_config_parse_font(name, text);
	}

	settings_reader_free(&reader);
	fclose(file);
}
