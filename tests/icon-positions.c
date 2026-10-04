#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/shared/string-util.h"
#include "../src/wm/config.h"
#include "../src/wm/icon-positions.h"

int
main(void)
{
	struct config config = {0};
	struct icon_position positions[2] = {{8, 8}, {8, 96}};
	FILE *file = NULL;
	char directory[] = "/tmp/owm-positions-test.XXXXXX";
	char path[PATH_MAX + 32];

	assert(mkdtemp(directory) != NULL);
	string_copy(config.directory, directory, sizeof(config.directory));
	string_copy(config.icons[0].id, "terminal", sizeof(config.icons[0].id));
	string_copy(config.icons[1].id, "browser", sizeof(config.icons[1].id));
	config.icon_count = 2;
	icon_positions_load(&config, positions);
	assert(positions[0].x == 8 && positions[1].y == 96);

	snprintf(path, sizeof(path), "%s/icon-positions", directory);
	file = fopen(path, "w");
	assert(file != NULL);
	fputs("terminal 120 40\n"
	"browser -1 50\n"
	"browser 1 999999999999999999999999\n"
	"browser 15 30 extra\n"
	"unknown 30 40\n"
	"terminal 240 80\n",
		file);
	assert(fclose(file) == 0);
	icon_positions_load(&config, positions);
	assert(positions[0].x == 240 && positions[0].y == 80);
	assert(positions[1].x == 8 && positions[1].y == 96);

	positions[1].x = 360;
	positions[1].y = 120;
	assert(icon_positions_save(&config, positions));
	/* The new file takes precedence over the original layout. */
	memset(positions, 0, sizeof(positions));
	icon_positions_load(&config, positions);
	assert(positions[0].x == 240 && positions[0].y == 80);
	assert(positions[1].x == 360 && positions[1].y == 120);
	assert(unlink(path) == 0);
	snprintf(path, sizeof(path), "%s/desktop-icon-positions", directory);
	assert(unlink(path) == 0);
	assert(rmdir(directory) == 0);
	puts("PASS icon-positions");

	return 0;
}
