#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wordexp.h>

#include "process.h"
#include "string-util.h"

static pid_t	process_spawn(int, char *const[]);

char *
process_quote_argv(int argc, char *const argv[], size_t limit)
{
	size_t length = 1;
	int i;
	char *p = NULL, *result = NULL;

	for (i = 0; i < argc; i++) {
		size_t n;

		n = strlen(argv[i]);

		if (length > limit || limit - length < 3 ||
			n > (limit - length - 3) / 4) {
			errno = E2BIG;
			return NULL;
		}

		length += 4 * n + 3;
	}

	result = malloc(length);

	if (result == NULL)
		return NULL;

	p = result;

	for (i = 0; i < argc; i++) {
		const char *s = NULL;

		if (i)
			*p++ = ' ';
		*p++ = '\'';

		for (s = argv[i]; (*s != '\0'); s++) {
			if (*s == '\'') {
				memcpy(p, "'\\''", 4);
				p += 4;
				continue;
			}

			*p++ = *s;
		}

		*p++ = '\'';
	}

	*p = '\0';

	return result;
}

static pid_t
process_spawn(int connection_fd, char *const argv[])
{
	ssize_t n;
	int failure, read_error;
	int pipefd[2];
	pid_t pid;

	if (pipe(pipefd) < 0)
		return -1;

	if (fcntl(pipefd[0], F_SETFD, FD_CLOEXEC) < 0 ||
		fcntl(pipefd[1], F_SETFD, FD_CLOEXEC) < 0) {
		int saved;

		saved = errno;

		close(pipefd[0]);
		close(pipefd[1]);
		errno = saved;

		return -1;
	}

	pid = fork();

	if (pid < 0) {
		int saved;

		saved = errno;

		close(pipefd[0]);
		close(pipefd[1]);
		errno = saved;

		return -1;
	}

	if (pid == 0) {
		ssize_t written;
		int saved;

		close(pipefd[0]);

		if (connection_fd >= 0)
			close(connection_fd);
		setsid();
		signal(SIGINT, SIG_DFL);
		signal(SIGTERM, SIG_DFL);
		signal(SIGHUP, SIG_DFL);
		execvp(argv[0], argv);

		saved = errno;

		do {
			written = write(pipefd[1], &saved, sizeof(saved));
		} while (written < 0 && errno == EINTR);
		_exit(127);
	}

	close(pipefd[1]);

	failure = 0;

	do {
		n = read(pipefd[0], &failure, sizeof(failure));
	} while (n < 0 && errno == EINTR);

	read_error = errno;

	close(pipefd[0]);

	if (n == 0)
		return pid;

	if (n > 0)
		while (waitpid(pid, NULL, 0) < 0 && errno == EINTR)
			continue;

	if (n == sizeof(failure))
		errno = failure;
	else if (n < 0)
		errno = read_error;
	else
		errno = EIO;

	return -1;
}

pid_t
process_shell(int connection_fd, const char *command)
{
	char *argv[4] = {0};

	argv[0] = "/bin/sh";
	argv[1] = "-c";
	argv[2] = (char *)command;
	argv[3] = NULL;

	return process_spawn(connection_fd, argv);
}

int
process_launch(int connection_fd, const char *command, char *error, size_t size)
{
	wordexp_t words = {0};
	int result, saved;
	pid_t pid;

	result = wordexp(command, &words, WRDE_NOCMD);

	if (result || words.we_wordc == 0) {
		string_copy(error,
			result ? "Invalid command or unsupported shell syntax."
			: "Enter a program name.",
			size);

		if (result == 0 || result == WRDE_NOSPACE)
			wordfree(&words);

		return 0;
	}

	pid = process_spawn(connection_fd, words.we_wordv);
	saved = errno;

	wordfree(&words);

	if (pid < 0)
		snprintf(error, size, "Cannot launch program: %s",
			strerror(saved));

	return pid >= 0;
}
