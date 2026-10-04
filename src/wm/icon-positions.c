#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "file-replace.h"
#include "icon-positions.h"
#include "paths.h"
#include "settings.h"

void
icon_positions_load(
	const struct config *config, struct icon_position *positions)
{
	FILE *file = NULL;
	size_t capacity;
	char *line = NULL;
	char path[PATH_MAX + 32];

	snprintf(path, sizeof(path), "%s/desktop-icon-positions",
		config->directory);

	file = fopen(path, "r");
	if (file == NULL && errno == ENOENT) {
		snprintf(path, sizeof(path), "%s/icon-positions",
			config->directory);
		file = fopen(path, "r");
	}

	if (file == NULL)
		return;

	capacity = 0;

	while (getline(&line, &capacity, file) >= 0) {
		int i, x, y;
		char extra, id[64], xs[32], ys[32];

		if (sscanf(line, "%63s %31s %31s %c", id, xs, ys, &extra) !=
				3 ||
			!parse_integer(xs, 0, WM_GEOMETRY_LIMIT, &x) ||
			!parse_integer(ys, 0, WM_GEOMETRY_LIMIT, &y))
			continue;

		for (i = 0; i < config->icon_count; i++)
			if (strcmp(id, config->icons[i].id) == 0) {
				positions[i].x = x;
				positions[i].y = y;
				break;
			}
	}

	free(line);
	fclose(file);
}

int
icon_positions_save(
	const struct config *config, const struct icon_position *positions)
{
	struct file_replace output = {0};
	int i;
	char path[PATH_MAX + 32];

	if (!directory_create(config->directory))
		return 0;

	snprintf(path, sizeof(path), "%s/desktop-icon-positions",
		config->directory);
	if (!file_replace_open(&output, path))
		return 0;

	for (i = 0; i < config->icon_count; i++)
		fprintf(output.stream, "%s %d %d\n", config->icons[i].id,
			positions[i].x, positions[i].y);

	return file_replace_commit(&output);
}
