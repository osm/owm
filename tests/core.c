#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <X11/keysym.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/shared/file-replace.h"
#include "../src/shared/memory.h"
#include "../src/shared/paths.h"
#include "../src/shared/process.h"
#include "../src/shared/settings.h"
#include "../src/shared/string-util.h"
#include "../src/shared/text-input.h"
#include "../src/shared/ui-config.h"
#include "../src/shared/utf8.h"

static void	wait_status(pid_t, int);
static void	check_file(const char *, const char *);
static void	test_settings_reader(void);
static void	test_allocation(void);
static void	test_file_replace(void);
static void	test_borrowed_ui(void);
static void	test_string_copy(void);
static void	test_utf8_boundaries(void);
static void	test_program_launch(void);
static void	test_boolean(void);
static void	test_process_quoting(void);
static void	test_process_errors(void);
static void	test_text_input(void);
static void	test_config_directory(void);
static void	test_font_config(void);

static void
wait_status(pid_t pid, int expected)
{
	int status;

	assert(pid > 0);

	assert(waitpid(pid, &status, 0) == pid);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == expected);
}

static void
test_utf8_boundaries(void)
{
	const char *text = "aå€🙂z";
	size_t i, j;
	size_t boundaries[] = {0, 1, 3, 6, 10, 11};

	assert(utf8_next("", 0) == 0);
	assert(utf8_previous("", 0) == 0);
	assert(utf8_floor("", 0) == 0);
	for (i = 0; i < 5; i++) {
		assert(utf8_next(text, boundaries[i]) == boundaries[i + 1]);
		assert(utf8_previous(text, boundaries[i + 1]) == boundaries[i]);
		for (j = boundaries[i]; j < boundaries[i + 1]; j++)
			assert(utf8_floor(text, j) == boundaries[i]);
	}
	assert(utf8_next(text, 11) == 11);
	assert(utf8_floor(text, 11) == 11);
	assert(utf8_previous(text, 0) == 0);
}

static void
test_string_copy(void)
{
	char buffer[5];

	memset(buffer, 'x', sizeof(buffer));
	assert(string_copy(NULL, "hello", 0) == 5);
	assert(string_copy(buffer, "hello", 0) == 5);
	assert(memcmp(buffer, "xxxxx", sizeof(buffer)) == 0);
	assert(string_copy(buffer, "hello", 1) == 5);
	assert(buffer[0] == '\0' && buffer[1] == 'x');
	assert(string_copy(buffer, "abcd", sizeof(buffer)) == 4);
	assert(strcmp(buffer, "abcd") == 0);
	assert(string_copy(buffer, "abcde", sizeof(buffer)) == 5);
	assert(strcmp(buffer, "abcd") == 0);
	assert(string_copy(buffer, "", sizeof(buffer)) == 0);
	assert(buffer[0] == '\0');
	memset(buffer, 'x', sizeof(buffer));
	assert(string_copy(buffer, "abcdef", 3) == 6);
	assert(memcmp(buffer, "ab\0xx", sizeof(buffer)) == 0);
}

static void
test_borrowed_ui(void)
{
	struct text_font font = {0};
	struct ui ui = {0};
	unsigned long colors[COLOR_COUNT] = {0};

	font.ascent = 11;
	colors[FACE] = 17;
	colors[ACTIVE] = 23;
	ui_borrow(&ui, NULL, NULL, &font, colors);
	assert(ui.colors == colors);
	assert(ui.font == &font && !ui.owns_resources);
	assert(ui.colors[FACE] == 17 && ui.colors[ACTIVE] == 23);
	ui_free(&ui);
	assert(ui.colors == NULL);
	assert(ui.font == NULL && ui.gc == NULL);
	assert(font.ascent == 11);
	ui_free(&ui);
}

static void
check_file(const char *path, const char *expected)
{
	FILE *file = NULL;
	char text[32];
	size_t length;

	file = fopen(path, "r");
	assert(file != NULL);
	length = fread(text, 1, sizeof(text), file);
	assert(!ferror(file) && feof(file));
	assert(length == strlen(expected));
	assert(memcmp(text, expected, length) == 0);
	assert(fclose(file) == 0);
}

static void
test_file_replace(void)
{
	struct file_replace output = {0};
	int fd;
	char directory[] = "/tmp/owm-file-replace.XXXXXX";
	char path[PATH_MAX], temporary[PATH_MAX + 32];

	assert(mkdtemp(directory) != NULL);
	snprintf(path, sizeof(path), "%s/state", directory);
	assert(file_replace_open(&output, path));
	fputs("original", output.stream);
	assert(file_replace_commit(&output));
	assert(output.stream == NULL && output.destination == NULL);

	assert(file_replace_open(&output, path));
	string_copy(temporary, output.temporary, sizeof(temporary));
	fputs("discarded", output.stream);
	errno = EDOM;
	file_replace_abort(&output);
	assert(errno == EDOM);
	assert(access(temporary, F_OK) < 0 && errno == ENOENT);
	check_file(path, "original");

	/* Failed writes must leave the existing destination untouched. */
	assert(file_replace_open(&output, path));
	string_copy(temporary, output.temporary, sizeof(temporary));
	fd = open("/dev/full", O_WRONLY);
	assert(fd >= 0);
	assert(dup2(fd, fileno(output.stream)) >= 0);
	assert(close(fd) == 0);
	fputs("replacement", output.stream);
	assert(!file_replace_commit(&output));
	assert(errno == ENOSPC);
	assert(access(temporary, F_OK) < 0 && errno == ENOENT);
	check_file(path, "original");

	assert(file_replace_open(&output, path));
	fputs("replacement", output.stream);
	assert(file_replace_commit(&output));
	check_file(path, "replacement");
	assert(unlink(path) == 0);

	/* A rename failure must also remove the temporary file. */
	assert(mkdir(path, 0700) == 0);
	assert(file_replace_open(&output, path));
	string_copy(temporary, output.temporary, sizeof(temporary));
	fputs("replacement", output.stream);
	assert(!file_replace_commit(&output));
	assert(access(temporary, F_OK) < 0 && errno == ENOENT);
	assert(rmdir(path) == 0);
	assert(rmdir(directory) == 0);
	assert(!file_replace_open(&output, path));
	assert(errno == ENOENT);
}

static void
test_allocation(void)
{
	size_t i;
	pid_t pid;
	unsigned char *bytes = NULL;

	bytes = allocate_array(7, 13);
	for (i = 0; i < 7 * 13; i++)
		assert(bytes[i] == 0);
	memset(bytes, 0xa5, 7 * 13);
	free(bytes);

	pid = fork();
	assert(pid >= 0);
	if (pid == 0) {
		assert(freopen("/dev/null", "w", stderr) != NULL);
		/* This product would wrap to zero before reaching calloc. */
		bytes = allocate_array((size_t)-1 / 2 + 1, 2);
		free(bytes);
		_exit(0);
	}
	wait_status(pid, 1);
}

static void
test_program_launch(void)
{
	FILE *file = NULL;
	int status;
	char directory[] = "/tmp/owm-launch.XXXXXX";
	char document[PATH_MAX], error[256], opener[PATH_MAX];
	char *saved_path = NULL;
	const char *path = NULL;

	path = getenv("PATH");
	if (path != NULL)
		saved_path = copy_string(path);

	assert(mkdtemp(directory) != NULL);
	snprintf(opener, sizeof(opener), "%s/xdg-open", directory);
	file = fopen(opener, "w");
	assert(file != NULL);
	assert(fputs("#!/bin/sh\nexit 0\n", file) >= 0);
	assert(fclose(file) == 0);
	assert(chmod(opener, 0700) == 0);
	assert(setenv("PATH", directory, 1) == 0);

	/* An available opener must never turn these inputs into launches. */
	assert(!process_launch(
		-1, "https://example.invalid", error, sizeof(error)));
	assert(!process_launch(
		-1, "mailto:test@example.invalid", error, sizeof(error)));
	assert(!process_launch(-1, directory, error, sizeof(error)));
	snprintf(document, sizeof(document), "%s/notes.txt", directory);
	file = fopen(document, "w");
	assert(file != NULL);
	assert(fclose(file) == 0);
	assert(!process_launch(-1, document, error, sizeof(error)));
	assert(!process_launch(-1, "", error, sizeof(error)));
	assert(!process_launch(-1, "'", error, sizeof(error)));

	assert(process_launch(-1,
		"/bin/sh -c 'test \"$1\" = \"two words\"' sh 'two words'",
		error, sizeof(error)));
	assert(waitpid(-1, &status, 0) > 0);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);

	if (saved_path != NULL)
		assert(setenv("PATH", saved_path, 1) == 0);
	else
		assert(unsetenv("PATH") == 0);
	free(saved_path);
	assert(unlink(document) == 0);
	assert(unlink(opener) == 0);
	assert(rmdir(directory) == 0);
}

static void
test_settings_reader(void)
{
	FILE *file = NULL;
	struct settings_reader reader = {0};
	char *decoded = NULL;
	const char invalid[] = "font = \"ok\"\0hidden\n";
	char *key = NULL, *value = NULL;

	file = tmpfile();

	assert(file != NULL);
	fputs(" \r\n  # ignored = value\n window { \r\n"
	" command = \"printf a=b # text\\n\" # comment\n"
	" }\n last = true",
		file);
	rewind(file);

	assert(settings_read_line(&reader, file) == 1);
	assert(reader.line_number == 3 && !strcmp(reader.text, "window {"));
	assert(!settings_split_assignment(reader.text, &key, &value));
	assert(settings_read_line(&reader, file) == 1);
	assert(reader.line_number == 4);
	assert(settings_split_assignment(reader.text, &key, &value));
	assert(!strcmp(key, "command"));

	decoded = decode_string(value);

	assert(decoded && !strcmp(decoded, "printf a=b # text\n"));
	free(decoded);
	assert(settings_read_line(&reader, file) == 1);
	assert(!strcmp(reader.text, "}"));
	assert(settings_read_line(&reader, file) == 1);
	assert(reader.line_number == 6);
	assert(settings_split_assignment(reader.text, &key, &value));
	assert(!strcmp(key, "last") && !strcmp(value, "true"));
	assert(settings_read_line(&reader, file) == 0);
	settings_reader_free(&reader);
	assert(reader.buffer == NULL && reader.text == NULL);
	assert(reader.capacity == 0 && reader.line_number == 0);
	settings_reader_free(&reader);
	fclose(file);

	file = tmpfile();
	assert(file != NULL);

	assert(fwrite(invalid, 1, sizeof(invalid) - 1, file) ==
		sizeof(invalid) - 1);
	rewind(file);
	assert(settings_read_line(&reader, file) == -1);
	assert(reader.line_number == 1 && errno == EINVAL);
	settings_reader_free(&reader);
	fclose(file);
	file = tmpfile();
	assert(file != NULL);
	fputs("# oversized comment\n", file);
	rewind(file);
	reader.max_length = 8;
	assert(settings_read_line(&reader, file) == -1);
	assert(reader.line_number == 1 && errno == EINVAL);
	settings_reader_free(&reader);
	assert(reader.max_length == 0);
	fclose(file);
}

static void
test_boolean(void)
{
	int flag = 0;

	assert(parse_boolean("true", &flag) && flag);
	assert(!parse_boolean("yes", &flag) && flag);
	assert(parse_boolean("false", &flag) && !flag);
}

static void
test_process_quoting(void)
{
	char *argv[5] = {0};
	char *quoted = NULL;
	const char *literal = NULL;

	literal = "apostrophe ' dollar $HOME semicolon ; newline\nvalue";

	assert(setenv("OWM_LITERAL", literal, 1) == 0);

	argv[0] = "/bin/sh";
	argv[1] = "-c";
	argv[2] = "test \"$1\" = \"$OWM_LITERAL\"";
	argv[3] = "test";
	argv[4] = (char *)literal;
	quoted = process_quote_argv(5, argv, 65536);

	assert(quoted != NULL);
	wait_status(process_shell(-1, quoted), 0);
	free(quoted);
	assert(!process_quote_argv(5, argv, 8) && errno == E2BIG);
}

static void
test_process_errors(void)
{
	struct rlimit limited, old;
	char error[256];

	assert(!process_launch(
		-1, "/owm-nonexistent-program", error, sizeof(error)));
	assert(strstr(error, strerror(ENOENT)) != NULL);
	wait_status(process_shell(-1, "exit 7"), 7);

	/*
	 * Descriptor exhaustion must report the real failure without leaking a
	 * pipe.
	 */

	assert(getrlimit(RLIMIT_NOFILE, &old) == 0);
	limited = old;
	limited.rlim_cur = 3;
	assert(setrlimit(RLIMIT_NOFILE, &limited) == 0);
	assert(process_shell(-1, "exit 0") == -1 && errno == EMFILE);
	assert(setrlimit(RLIMIT_NOFILE, &old) == 0);
}

static void
test_text_input(void)
{
	struct text_input entry = {0};
	char long_utf8[TEXT_INPUT_SIZE + 4];

	assert(text_input_insert(&entry, "old command", 11));
	text_input_key(&entry, XK_a, ControlMask, "", 0);
	assert(text_input_insert(&entry, "new", 3));
	assert(!strcmp(entry.text, "new") && entry.cursor == 3);
	assert(!text_input_insert(&entry, "ignored", TEXT_INPUT_SIZE));
	assert(!strcmp(entry.text, "new"));
	text_input_key(&entry, XK_a, ControlMask, "", 0);
	assert(text_input_insert(&entry, "aå🙂z", strlen("aå🙂z")));
	text_input_key(&entry, XK_End, 0, "", 0);
	text_input_key(&entry, XK_Left, 0, "", 0);
	assert(entry.cursor == 7);
	text_input_key(&entry, XK_Left, 0, "", 0);
	assert(entry.cursor == 3);
	text_input_key(&entry, XK_BackSpace, 0, "", 0);
	assert(!strcmp(entry.text, "a🙂z") && entry.cursor == 1);
	text_input_key(&entry, XK_Delete, 0, "", 0);
	assert(!strcmp(entry.text, "az"));
	text_input_key(&entry, XK_Right, 0, "", 0);
	assert(entry.cursor == 2);

	memset(long_utf8, 'a', TEXT_INPUT_SIZE - 2);
	string_copy(long_utf8 + TEXT_INPUT_SIZE - 2, "🙂",
		sizeof(long_utf8) - (TEXT_INPUT_SIZE - 2));
	text_input_key(&entry, XK_a, ControlMask, "", 0);
	assert(!text_input_insert(&entry, long_utf8, strlen(long_utf8)));
	assert(!strcmp(entry.text, "az"));
	assert(text_input_insert(&entry, long_utf8, TEXT_INPUT_SIZE - 2));
	assert(!text_input_insert(&entry, "🙂", strlen("🙂")));
	assert(text_input_insert(&entry, "z", 1));
	assert(strlen(entry.text) == TEXT_INPUT_SIZE - 1);
}

static void
test_config_directory(void)
{
	char current[PATH_MAX], resolved[PATH_MAX];
	char long_directory[PATH_MAX + 1];
	char temp[] = "/tmp/owm-paths.XXXXXX";

	memset(long_directory, 'a', sizeof(long_directory) - 1);
	long_directory[0] = '/';
	long_directory[sizeof(long_directory) - 1] = '\0';
	assert(setenv("XDG_CONFIG_HOME", long_directory, 1) == 0);
	assert(!config_directory(resolved, sizeof(resolved)));
	assert(errno == ENAMETOOLONG);

	assert(mkdtemp(temp) != NULL);
	assert(setenv("XDG_CONFIG_HOME", temp, 1) == 0);
	assert(!config_directory(resolved, 1));
	assert(errno == ENAMETOOLONG);
	snprintf(current, sizeof(current), "%s/owm", temp);
	assert(config_directory(resolved, sizeof(resolved)) &&
		!strcmp(resolved, current));
	assert(mkdir(current, 0700) == 0);
	assert(config_directory(resolved, sizeof(resolved)) &&
		!strcmp(resolved, current));
	assert(rmdir(current) == 0);
	assert(rmdir(temp) == 0);
}

static void
test_font_config(void)
{
	FILE *config = NULL;
	char config_path[PATH_MAX], current[PATH_MAX], font[FONT_NAME_SIZE];
	char temp[] = "/tmp/owm-font-config.XXXXXX";

	assert(mkdtemp(temp) != NULL);
	assert(setenv("XDG_CONFIG_HOME", temp, 1) == 0);
	snprintf(current, sizeof(current), "%s/owm", temp);
	assert(mkdir(current, 0700) == 0);
	ui_config_font(font);
	assert(!strcmp(font, DEFAULT_FONT));
	snprintf(config_path, sizeof(config_path), "%s/owm/config", temp);

	config = fopen(config_path, "w");

	assert(config != NULL);
	fputs("# font = \"ignored\"\nfont = \"Liberation Mono:size=16\" # "
	"comment\n"
	"font = \"\"\nfont = invalid\n",
		config);
	assert(fclose(config) == 0);
	ui_config_font(font);
	assert(!strcmp(font, "Liberation Mono:size=16"));
	assert(unlink(config_path) == 0);
	assert(rmdir(current) == 0);
	assert(rmdir(temp) == 0);
}

int
main(void)
{
	test_allocation();
	test_file_replace();
	test_borrowed_ui();
	test_string_copy();
	test_utf8_boundaries();
	test_settings_reader();
	test_boolean();
	test_program_launch();
	test_process_quoting();
	test_process_errors();
	test_text_input();
	test_config_directory();
	test_font_config();
	puts("PASS core");

	return 0;
}
