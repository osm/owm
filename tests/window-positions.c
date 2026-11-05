#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/wm/window-positions.h"

static void	test_roundtrip(void);
static void	test_invalid(void);

static void
test_roundtrip(void)
{
	struct window_position first = {0}, second = {0};
	struct window_position *records = NULL;
	FILE *file = NULL;

	first.class_name = "Game \"quoted\"";
	first.instance = "café\n\\instance";
	first.x = -100;
	first.y = 250;
	first.next = &second;
	second.class_name = "";
	second.instance = "other";
	second.x = 50;
	second.y = 80;
	file = tmpfile();
	assert(file != NULL);
	assert(window_positions_write(file, &first));
	rewind(file);
	assert(window_positions_read(file, &records));
	assert(records != NULL && records->next != NULL);
	assert(records->next->next == NULL);
	assert(strcmp(records->class_name, first.class_name) == 0);
	assert(strcmp(records->instance, first.instance) == 0);
	assert(records->x == -100 && records->y == 250);
	assert(strcmp(records->next->instance, "other") == 0);
	assert(records->next->x == 50 && records->next->y == 80);
	window_positions_free(&records);
	assert(records == NULL);
	assert(fclose(file) == 0);
}

static void
test_invalid(void)
{
	struct window_position *records = NULL;
	FILE *file = NULL;
	size_t i;
	static const char *invalid[] = {"position {\nclass = \"truncated\"\n",
		"position {\nclass = \"\"\ninstance = \"\"\nx = 1\ny = 2\n}\n",
		"position {\nclass = \"Game\"\ninstance = \"game\"\nx = 1\n}\n",
		"position {\nclass = \"Game\"\ninstance = \"game\"\nx = "
		"999999\n}",
		"position {\nx = 1\nx = 2\n}",
		"position {\nclass = unquoted\n}", "position {\nunknown = 1\n}",
		"}\n"};

	for (i = 0; i < sizeof(invalid) / sizeof(*invalid); i++) {
		file = tmpfile();
		assert(file != NULL);
		/* A bad trailing record must discard all earlier records. */
		assert(fputs("position {\nclass = \"Valid\"\ninstance = \"\"\n"
			"x = 1\ny = 2\n}\n",
			file) >= 0);
		assert(fputs(invalid[i], file) >= 0);
		rewind(file);
		assert(!window_positions_read(file, &records));
		assert(records == NULL);
		assert(fclose(file) == 0);
	}
}

int
main(void)
{
	test_roundtrip();
	test_invalid();
	puts("PASS window-positions");

	return 0;
}
