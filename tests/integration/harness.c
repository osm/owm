#include <dlfcn.h>
#include <stdarg.h>

#include "harness.h"

Display *dpy = NULL;
Window root;
pid_t wm_pid;
char fixture_path[PATH_MAX], output_path[PATH_MAX];
char config_path[PATH_MAX + 32], session_path[PATH_MAX + 32];

int (*fake_motion)(Display *, int, int, int, unsigned long) = NULL;
int (*fake_button)(Display *, unsigned int, Bool, unsigned long) = NULL;
int (*fake_key)(Display *, unsigned int, Bool, unsigned long) = NULL;

static void *xtst = NULL;

void
write_config(const char *format, ...)
{
	FILE *file = NULL;
	va_list args;
	int written;

	file = fopen(config_path, "w");
	CHECK(file != NULL);
	va_start(args, format);
	written = vfprintf(file, format, args);
	va_end(args);
	CHECK(written >= 0);
	CHECK(fclose(file) == 0);
}

int
log_contains(const char *expected)
{
	FILE *file = NULL;
	char path[PATH_MAX + 32], line[4096];
	int found = 0;

	snprintf(path, sizeof(path), "%s/wm.log", output_path);
	file = fopen(path, "r");
	CHECK(file != NULL);
	while (fgets(line, sizeof(line), file) != NULL)
		if (strstr(line, expected) != NULL) {
			found = 1;
			break;
		}
	CHECK(!ferror(file));
	CHECK(fclose(file) == 0);

	return found;
}

void
pause_ms(int ms)
{
	struct timespec t = {0};

	t.tv_sec = ms / 1000;
	t.tv_nsec = (ms % 1000) * 1000000L;

	while (nanosleep(&t, &t) < 0 && errno == EINTR)
		continue;
}

void
cleanup(void)
{
	if (wm_pid > 0) {
		kill(wm_pid, SIGTERM);
		waitpid(wm_pid, NULL, 0);
		wm_pid = 0;
	}

	if (dpy != NULL) {
		XCloseDisplay(dpy);
		dpy = NULL;
	}
	if (xtst != NULL) {
		dlclose(xtst);
		xtst = NULL;
	}
}

void
fail(const char *condition, const char *file, int line)
{
	fprintf(stderr, "FAIL %s:%d: %s (logs: %s)\n", file, line, condition,
		output_path);
	exit(1);
}

pid_t
spawn(const char *program)
{
	pid_t pid;

	pid = fork();

	CHECK(pid >= 0);

	if (pid == 0) {
		int fd;
		char path[PATH_MAX + 32];

		close(ConnectionNumber(dpy));

		snprintf(path, sizeof(path), "%s/wm.log", output_path);

		fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0600);

		if (fd >= 0) {
			dup2(fd, STDERR_FILENO);
			close(fd);
		}

		execl(program, program, (char *)NULL);
		_exit(127);
	}

	return pid;
}

int
wait_exit(pid_t pid)
{
	int i;
	int status = 0;
	pid_t result = 0;

	for (i = 0; i < 1500 && result == 0; i++) {
		result = waitpid(pid, &status, WNOHANG);

		if (result == 0)
			pause_ms(20);
	}

	CHECK(result == pid);
	CHECK(WIFEXITED(status));

	return WEXITSTATUS(status);
}

void
start_wm(void)
{
	FILE *config = NULL;

	/* Keep session saving available regardless of the case's shortcuts. */
	config = fopen(config_path, "a");
	CHECK(config != NULL);
	CHECK(fputs("\nbind = \"Ctrl+Shift+F11 session_save\"\n", config) >= 0);
	CHECK(fclose(config) == 0);

	CHECK(value(root, "_NET_SUPPORTING_WM_CHECK", XA_WINDOW) == None);
	wm_pid = spawn("./owm");
	WAIT(value(root, "_NET_SUPPORTING_WM_CHECK", XA_WINDOW) != None);
	pause_ms(80);
}

void
save_session(void)
{
	struct stat before = {0}, after;

	if (stat(session_path, &before) < 0)
		CHECK(errno == ENOENT);
	shortcut(XK_F11, ControlMask | ShiftMask);
	/* Saving replaces the file atomically, even if its contents match. */
	WAIT(stat(session_path, &after) == 0 &&
		(after.st_dev != before.st_dev ||
			after.st_ino != before.st_ino));
}

void
stop_wm(void)
{
	CHECK(kill(wm_pid, SIGTERM) == 0);
	CHECK(wait_exit(wm_pid) == 0);
	wm_pid = 0;
	CHECK(value(root, "_NET_SUPPORTING_WM_CHECK", XA_WINDOW) == None);
}

void
harness_init(const char *name)
{
	void *symbol = NULL;
	char config[PATH_MAX + 32];
	int attempt;

	CHECK(getenv("OWM_TEST_DISPLAY"));
	CHECK(snprintf(output_path, sizeof(output_path), "%s/%s.XXXXXX",
		getenv("OWM_TEST_OUTPUT"),
		name) < (int)sizeof(output_path));
	CHECK(mkdtemp(output_path) != NULL);
	CHECK(setenv("OWM_TEST_OUTPUT", output_path, 1) == 0);
	CHECK(snprintf(config, sizeof(config), "%s/config", output_path) <
		(int)sizeof(config));
	CHECK(mkdir(config, 0700) == 0);
	CHECK(setenv("XDG_CONFIG_HOME", config, 1) == 0);
	CHECK(snprintf(config, sizeof(config), "%s/config/owm", output_path) <
		(int)sizeof(config));
	CHECK(mkdir(config, 0700) == 0);
	CHECK(snprintf(config_path, sizeof(config_path), "%s/config", config) <
		(int)sizeof(config_path));
	CHECK(snprintf(session_path, sizeof(session_path), "%s/session",
		config) < (int)sizeof(session_path));
	CHECK(getcwd(fixture_path, sizeof(fixture_path)));
	CHECK(strlen(fixture_path) + strlen("/build/tests/integration") <
		sizeof(fixture_path));
	string_copy(fixture_path + strlen(fixture_path),
		"/build/tests/integration",
		sizeof(fixture_path) - strlen(fixture_path));
	/* Tolerate brief display unavailability between isolated cases. */
	for (attempt = 0; attempt < 100; attempt++) {
		dpy = XOpenDisplay(NULL);
		if (dpy != NULL)
			break;
		pause_ms(20);
	}
	CHECK(dpy);
	root = DefaultRootWindow(dpy);
	fcntl(ConnectionNumber(dpy), F_SETFD, FD_CLOEXEC);
	atexit(cleanup);

	xtst = dlopen("libXtst.so.6", RTLD_NOW);

	CHECK(xtst);

	symbol = dlsym(xtst, "XTestFakeMotionEvent");

	memcpy(&fake_motion, &symbol, sizeof(fake_motion));
	symbol = dlsym(xtst, "XTestFakeButtonEvent");
	memcpy(&fake_button, &symbol, sizeof(fake_button));
	symbol = dlsym(xtst, "XTestFakeKeyEvent");
	memcpy(&fake_key, &symbol, sizeof(fake_key));
	CHECK(fake_motion && fake_button && fake_key);

	/*
	 * Root placement is persistent across WM restarts, but not test cases.
	 */
	XDeleteProperty(dpy, root, atom("_OWM_CURRENT_DESKTOP"));
	XSync(dpy, False);
}
