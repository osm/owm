#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "memory.h"
#include "process.h"
#include "wm.h"

static char	*read_command(Window);
static int	 client_is_local(const struct client *);
static char	*read_command_data(int, size_t *);
static char	*build_process_command(char *, size_t, const char *, char *);
static char	*read_process_command(struct client *);

static char *
read_command(Window window)
{
	int argc = 0;
	char **argv = NULL;
	char *command = NULL;

	if (XGetCommand(wm.display, window, &argv, &argc) == 0 || argc < 1) {
		if (argv != NULL)
			XFreeStringList(argv);
		return NULL;
	}

	command = (*argv[0] != '\0') ? process_quote_argv(argc, argv, MAX_TEXT)
				: NULL;

	XFreeStringList(argv);

	return command;
}

static int
client_is_local(const struct client *c)
{
	XTextProperty machine = {0};
	int local;
	char hostname[256];

	if (gethostname(hostname, sizeof(hostname)) ||
		memchr(hostname, '\0', sizeof(hostname)) == NULL)
		return 0;

	if (XGetWMClientMachine(wm.display, c->window, &machine) == 0)
		return 0;

	local = machine.value != NULL && machine.encoding == XA_STRING &&
		machine.format == 8 && machine.nitems == strlen(hostname) &&
		memcmp(machine.value, hostname, machine.nitems) == 0;

	if (machine.value != NULL)
		XFree(machine.value);

	return local;
}

/* Borrow fd. Return an owned, validated NUL separated argument buffer. */
static char *
read_command_data(int fd, size_t *length)
{
	size_t used = 0;
	ssize_t n = 0;
	char *data = NULL;

	*length = 0;
	data = allocate(MAX_TEXT + 1);

	while (used <= MAX_TEXT) {
		n = read(fd, data + used, MAX_TEXT + 1 - used);

		if (n < 0 && errno == EINTR)
			continue;

		if (n <= 0)
			break;

		used += (size_t)n;

		if (used > MAX_TEXT)
			break;
	}

	if (n < 0 || used == 0 || used > MAX_TEXT || data[used - 1] != '\0' ||
		*data == '\0') {
		free(data);
		return NULL;
	}

	*length = used;

	return data;
}

/* Borrow validated argument data and paths. Return an owned shell command. */
static char *
build_process_command(
	char *data, size_t length, const char *cwd, char *executable)
{
	size_t i, size;
	int argc = 0;
	char **argv = NULL;
	char *command = NULL, *quoted_cwd = NULL, *result = NULL;
	char *cwd_argv[1] = {0};

	argv = allocate_array(length + 1, sizeof(*argv));

	for (i = 0; i < length; i += strlen(data + i) + 1)
		argv[argc++] = data + i;

	argv[0] = executable;
	command = process_quote_argv(argc, argv, MAX_TEXT);
	free(argv);

	if (command == NULL)
		return NULL;

	cwd_argv[0] = (char *)cwd;
	quoted_cwd = process_quote_argv(1, cwd_argv, MAX_TEXT);

	if (quoted_cwd == NULL) {
		free(command);
		return NULL;
	}

	size = strlen(command) + strlen(quoted_cwd) + 16;

	if (size <= MAX_TEXT) {
		result = allocate(size);
		snprintf(result, size, "cd %s && exec %s", quoted_cwd, command);
	}

	free(quoted_cwd);
	free(command);

	return result;
}

/*
 * Some clients (including st) publish a local PID but omit WM_COMMAND.
 * Resolve only processes owned by the same user with an explicitly matching
 * hostname.
 */
static char *
read_process_command(struct client *c)
{
	struct stat st;
	size_t length;
	ssize_t cwd_length, executable_length;
	int directory, fd;
	char cwd[PATH_MAX], executable[PATH_MAX], path[64];
	char *data = NULL, *result = NULL;

	if (c->pid == 0 || c->pid > INT_MAX || !client_is_local(c))
		return NULL;

	snprintf(path, sizeof(path), "/proc/%lu", c->pid);
	directory = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);

	if (directory < 0)
		return NULL;

	if (fstat(directory, &st) || st.st_uid != getuid()) {
		close(directory);
		return NULL;
	}

	executable_length = readlinkat(
		directory, "exe", executable, sizeof(executable) - 1);
	cwd_length = readlinkat(directory, "cwd", cwd, sizeof(cwd) - 1);

	if (executable_length <= 0 ||
		executable_length >= (ssize_t)sizeof(executable) - 1 ||
		cwd_length <= 0 || cwd_length >= (ssize_t)sizeof(cwd) - 1) {
		close(directory);
		return NULL;
	}

	executable[executable_length] = cwd[cwd_length] = '\0';

	if (executable[0] != '/' || cwd[0] != '/' || access(executable, X_OK)) {
		close(directory);
		return NULL;
	}

	fd = openat(directory, "cmdline", O_RDONLY | O_CLOEXEC);
	close(directory);

	if (fd < 0)
		return NULL;

	data = read_command_data(fd, &length);
	close(fd);

	if (data == NULL)
		return NULL;

	result = build_process_command(data, length, cwd, executable);
	free(data);

	return result;
}

char *
session_command_capture(struct client *client)
{
	char *command = NULL;

	command = read_command(client->window);

	if (command == NULL &&
		(client->command == NULL || *client->command == '\0'))
		command = read_process_command(client);

	return command;
}
