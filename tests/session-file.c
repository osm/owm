#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/wm/session-file.h"

static void	test_roundtrip(void);
static void	test_invalid_records(void);
static void	test_empty_session(void);
static int	read_text(const char *, int *, struct session_record **);
static void	read_invalid(const char *, int *, struct session_record **,
	unsigned int);

static int
read_text(const char *text, int *desktop, struct session_record **records)
{
	FILE *file = NULL;
	int ok;

	file = tmpfile();
	assert(file != NULL);
	assert(fputs(text, file) >= 0);
	rewind(file);
	ok = session_file_read(file, "test session", desktop, records);
	assert(fclose(file) == 0);

	return ok;
}

static void
read_invalid(const char *text, int *desktop, struct session_record **records,
	unsigned int line)
{
	FILE *errors = NULL;
	char output[256], expected[64];
	int saved, ok;
	size_t length;

	errors = tmpfile();
	assert(errors != NULL);
	assert(fflush(stderr) == 0);
	saved = dup(STDERR_FILENO);
	assert(saved >= 0);
	assert(dup2(fileno(errors), STDERR_FILENO) >= 0);
	ok = read_text(text, desktop, records);
	assert(fflush(stderr) == 0);
	assert(dup2(saved, STDERR_FILENO) >= 0);
	assert(close(saved) == 0);
	rewind(errors);
	length = fread(output, 1, sizeof(output) - 1, errors);
	output[length] = '\0';
	assert(fclose(errors) == 0);
	snprintf(expected, sizeof(expected), "test session:%u: invalid session",
		line);
	assert(!ok && *records == NULL);
	assert(strstr(output, expected) != NULL);
}

static void
test_roundtrip(void)
{
	struct session_record *records = NULL, *roundtrip = NULL;
	FILE *file = NULL;
	int desktop;
	static const char text[] = "current_desktop = 3\n"
				"window {\n"
				"class = \"Terminal\"\n"
				"instance = \"later\"\n"
				"title = \"café\\nconsole\"\n"
				"command = \"printf 'a b' \\\"quoted\\\"\"\n"
				"x = -48\n"
				"y = 120\n"
				"width = 640\n"
				"height = 480\n"
				"minimized = true\n"
				"maximized_horizontal = true\n"
				"fullscreen = true\n"
				"desktop = 0\n"
				"order = 5\n"
				"}\n"
				"window {\n"
				"instance = \"first\"\n"
				"x = 1\n"
				"y = 2\n"
				"width = 3\n"
				"height = 4\n"
				"order = 1\n"
				"}\n";

	desktop = 1;
	assert(read_text(text, &desktop, &records));
	assert(desktop == 3 && records != NULL && records->next != NULL);
	assert(strcmp(records->instance, "first") == 0);
	assert(strcmp(records->next->title, "café\nconsole") == 0);
	assert(strcmp(records->next->command, "printf 'a b' \"quoted\"") == 0);
	assert(records->next->desktop == -1);
	file = tmpfile();
	assert(file != NULL);
	assert(session_file_write(file, desktop, records));
	assert(records->order == 1 && records->next->order == 5);
	rewind(file);
	desktop = 1;
	assert(session_file_read(file, "roundtrip", &desktop, &roundtrip));
	assert(fclose(file) == 0);
	assert(desktop == 3 && roundtrip != NULL && roundtrip->next != NULL);
	assert(roundtrip->next->next == NULL);
	assert(strcmp(roundtrip->next->command, records->next->command) == 0);
	assert(strcmp(roundtrip->next->title, records->next->title) == 0);
	assert(roundtrip->next->g.x == -48 && roundtrip->next->g.y == 120);
	assert(roundtrip->next->g.w == 640 && roundtrip->next->g.h == 480);
	assert(roundtrip->next->minimized && roundtrip->next->fullscreen);
	assert(roundtrip->next->max_h && !roundtrip->next->max_v);
	assert(roundtrip->next->desktop == -1 && roundtrip->next->order == 5);
	session_records_free(&records);
	session_records_free(&roundtrip);
	assert(records == NULL && roundtrip == NULL);
}

static void
test_invalid_records(void)
{
	struct session_record *records = NULL;
	int desktop;

	/* A late parse failure must discard earlier accepted records, too. */
	desktop = 2;
	read_invalid("window {\n"
		"instance = \"valid\"\n"
		"x = 1\n"
		"y = 2\n"
		"width = 3\n"
		"height = 4\n"
		"}\n"
		"window {\n"
		"instance = \"bad\"\n"
		"x = 0\n"
		"y = 0\n"
		"width = -1\n"
		"height = 20\n"
		"}\n",
		&desktop, &records, 12);
	read_invalid("window {\n"
		"instance = \"truncated\"\n",
		&desktop, &records, 2);
}

static void
test_empty_session(void)
{
	struct session_record *records = NULL;
	int desktop = 2;

	assert(read_text("# empty session\n", &desktop, &records));
	assert(records == NULL && desktop == 2);
}

int
main(void)
{
	test_roundtrip();
	test_invalid_records();
	test_empty_session();
	puts("PASS session-file");

	return 0;
}
