/* WAIT must observe a successful condition only once. */
#include <assert.h>

#include "integration/harness.h"

static int pauses, expect_timeout;

void
pause_ms(int ms)
{
	assert(ms == 20);
	pauses++;
}

void
fail(const char *condition, const char *file, int line)
{
	assert(expect_timeout);
	assert(strcmp(condition, "0") == 0);
	assert(strcmp(file, __FILE__) == 0 && line > 0);
	assert(pauses == 150);
	exit(0);
}

int
main(void)
{
	pid_t pid;
	int attempts = 0, status;

	WAIT(++attempts == 1);
	assert(attempts == 1 && pauses == 0);
	WAIT(++attempts == 4);
	assert(attempts == 4 && pauses == 2);

	pid = fork();
	assert(pid >= 0);
	if (pid == 0) {
		expect_timeout = 1;
		pauses = 0;
		WAIT(0);
		_exit(1);
	}
	assert(waitpid(pid, &status, 0) == pid);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	puts("PASS wait");

	return 0;
}
