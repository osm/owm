#include "cases.h"
#include "harness.h"

void
test_session_identity(void)
{
	Window duplicate_a, duplicate_b;
	unsigned long n, *order = NULL;

	/* Titles distinguish windows sharing the same class and instance. */
	start_wm();

	duplicate_a = make_window("same", "Duplicate A", 0);
	duplicate_b = make_window("same", "Duplicate B", 0);

	XMapWindow(dpy, duplicate_a);
	XMapWindow(dpy, duplicate_b);
	XFlush(dpy);
	WAIT(count_clients() == 2);
	XMoveWindow(dpy, duplicate_a, 100, 200);
	XMoveWindow(dpy, duplicate_b, 700, 300);
	XFlush(dpy);
	WAIT(attributes(parent_of(duplicate_a)).x == 100 &&
		attributes(parent_of(duplicate_b)).x == 700);
	save_session();
	stop_wm();
	XDestroyWindow(dpy, duplicate_a);
	XDestroyWindow(dpy, duplicate_b);
	XSync(dpy, False);

	/* Recreate in the opposite order, before WM adoption. */
	duplicate_b = make_window("same", "Duplicate B", 0);
	duplicate_a = make_window("same", "Duplicate A", 0);
	XMapWindow(dpy, duplicate_b);
	XMapWindow(dpy, duplicate_a);
	XSync(dpy, False);
	start_wm();
	WAIT(count_clients() == 2);
	CHECK(attributes(parent_of(duplicate_a)).x == 100 &&
		attributes(parent_of(duplicate_b)).x == 700);
	order = property(root, "_NET_CLIENT_LIST", XA_WINDOW, &n);
	CHECK(n == 2 && order[0] == duplicate_a && order[1] == duplicate_b);
	XFree(order);
	XDestroyWindow(dpy, duplicate_a);
	XDestroyWindow(dpy, duplicate_b);
	XFlush(dpy);
	WAIT(count_clients() == 0);
	stop_wm();
}

void
test_invalid_session(void)
{
	FILE *bad = NULL;

	bad = fopen(session_path, "w");

	CHECK(bad);
	fputs("window {\n"
	"class = \"Bad\"\n"
	"instance = \"bad\"\n"
	"command = \"exit 99\"\n"
	"x = 1\n"
	"y = 1\n"
	"width = -1\n"
	"height = 20\n"
	"}\n",
		bad);
	CHECK(fclose(bad) == 0);
	start_wm();
	CHECK(count_clients() == 0);
	save_session();

	stop_wm();
}

void
test_xterm_restore(void)
{
	Window quake, kernel;
	pid_t xterm_a, xterm_b;
	int ignored;

	start_wm();

	xterm_a = start_xterm("quake", "Quake console\nReady.\n");
	xterm_b = start_xterm("kernel",
		"Kernel messages\nX11 window-manager integration test\n");

	WAIT(count_clients() == 2);
	quake = find_instance("quake");
	kernel = find_instance("kernel");
	CHECK(quake && kernel);
	XMoveWindow(dpy, quake, 40, 60);
	XMoveWindow(dpy, kernel, 540, 150);
	XFlush(dpy);
	WAIT(attributes(parent_of(quake)).x == 40 &&
		attributes(parent_of(kernel)).x == 540);
	save_session();
	pause_ms(200);
	screenshot("desktop");
	stop_wm();
	CHECK(parent_of(quake) == root && parent_of(kernel) == root);
	start_wm();
	WAIT(count_clients() == 2);
	CHECK(find_instance("quake") == quake &&
		find_instance("kernel") == kernel);
	stop_wm();
	CHECK(kill(xterm_a, SIGTERM) == 0 && kill(xterm_b, SIGTERM) == 0);

	CHECK(waitpid(xterm_a, &ignored, 0) == xterm_a &&
		waitpid(xterm_b, &ignored, 0) == xterm_b);
	start_wm();
	WAIT(count_clients() == 2);
	quake = find_instance("quake");
	kernel = find_instance("kernel");
	CHECK(quake && kernel);
	CHECK(attributes(parent_of(quake)).x == 40 &&
		attributes(parent_of(kernel)).x == 540);
	message(quake, "_NET_CLOSE_WINDOW", CurrentTime, 0, 0);
	message(kernel, "_NET_CLOSE_WINDOW", CurrentTime, 0, 0);
	WAIT(count_clients() == 0);
	stop_wm();
}

void
test_st_restore(void)
{
	XWindowAttributes original;
	Window st;
	int ignored;
	pid_t terminal;
	char candidate[PATH_MAX], program[PATH_MAX] = "";
	char *dir = NULL;
	char *paths = NULL;
	char *save = NULL;

	paths = strdup(
		(getenv("PATH") != NULL) ? getenv("PATH") : "/usr/bin:/bin");

	CHECK(paths);

	save = NULL;

	for (dir = strtok_r(paths, ":", &save); (dir != NULL);
		dir = strtok_r(NULL, ":", &save)) {
		int n;

		n = snprintf(candidate, sizeof(candidate), "%s/st", dir);

		if (n > 0 && (size_t)n < sizeof(candidate) &&
			access(candidate, X_OK) == 0) {
			string_copy(program, candidate, sizeof(program));
			break;
		}
	}

	free(paths);

	if (*program == '\0') {
		fputs("st is not installed\n", stderr);
		exit(TEST_SKIP);
	}

	CHECK(access(session_path, F_OK) < 0 && errno == ENOENT);
	start_wm();

	terminal = fork();

	CHECK(terminal >= 0);

	if (terminal == 0) {
		close(ConnectionNumber(dpy));
		execl(program, "st", "-n", "owm-st-restore", "-t",
			"Saved st terminal", (char *)NULL);
		_exit(127);
	}

	WAIT(find_instance("owm-st-restore"));

	st = find_instance("owm-st-restore");

	XMoveResizeWindow(dpy, st, 130, 170, 520, 350);
	XFlush(dpy);
	WAIT(attributes(parent_of(st)).x == 130 &&
		attributes(parent_of(st)).y == 170);

	original = attributes(st);

	save_session();
	stop_wm();
	CHECK(kill(terminal, SIGTERM) == 0);

	CHECK(waitpid(terminal, &ignored, 0) == terminal);
	start_wm();
	WAIT(find_instance("owm-st-restore"));
	st = find_instance("owm-st-restore");
	CHECK(attributes(parent_of(st)).x == 130 &&
		attributes(parent_of(st)).y == 170);
	CHECK(attributes(st).width == original.width &&
		attributes(st).height == original.height);
	CHECK(count_clients() == 1);

	/* Saving a restored fallback command must remain restartable. */
	save_session();
	message(st, "_NET_CLOSE_WINDOW", CurrentTime, 0, 0);
	WAIT(count_clients() == 0);
	stop_wm();
	start_wm();
	WAIT(find_instance("owm-st-restore"));
	message(find_instance("owm-st-restore"), "_NET_CLOSE_WINDOW",
		CurrentTime, 0, 0);
	WAIT(count_clients() == 0);
	stop_wm();
}

void
test_saved_commands(void)
{
	FILE *file = NULL;
	size_t length;
	Window windows[4];
	int i, pass;
	char contents[16384];
	const char *titles[] = {"Saved A", "Saved B", "Unrelated", "Captured"};

	write_config("restore_session = false\n");
	start_wm();
	for (i = 0; i < 4; i++) {
		windows[i] =
			make_window(i < 2 ? "saved" : titles[i], titles[i], 0);
		if (i < 3)
			XDeleteProperty(dpy, windows[i], XA_WM_COMMAND);
		XMapWindow(dpy, windows[i]);
	}
	XFlush(dpy);
	WAIT(count_clients() == 4);

	file = fopen(session_path, "w");
	CHECK(file != NULL);
	fputs("current_desktop = 4\n", file);
	for (i = 0; i < 4; i++) {
		if (i == 2)
			continue;
		fprintf(file,
			"window {\nclass = \"OwmTest\"\n"
			"instance = \"%s\"\ntitle = \"%s\"\n"
			"command = \"true # %s\"\n"
			"x = 1\ny = 1\nwidth = 100\nheight = 100\n}\n",
			i < 2 ? "saved" : titles[i], titles[i], titles[i]);
	}
	CHECK(fclose(file) == 0);

	for (pass = 0; pass < 3; pass++) {
		if (pass == 1) {
			XStoreName(dpy, windows[0], "Changed A");
			XFlush(dpy);
		}
		if (pass == 2) {
			stop_wm();
			start_wm();
			WAIT(count_clients() == 4);
		}
		save_session();
		file = fopen(session_path, "r");
		CHECK(file != NULL);
		length = fread(contents, 1, sizeof(contents) - 1, file);
		CHECK(!ferror(file) && feof(file));
		contents[length] = '\0';
		CHECK(fclose(file) == 0);
		CHECK(strstr(contents,
			pass == 0 ? "title = \"Saved A\"\n    command = "
					"\"true # Saved A\""
					: "title = \"Changed A\"\n    command "
					"= \"true # Saved A\"") != NULL);
		CHECK(strstr(contents,
			"title = \"Saved B\"\n    command = \"true # "
			"Saved B\"") != NULL);
		CHECK(strstr(contents,
			"title = \"Unrelated\"\n    command = \"\"") !=
			NULL);
		CHECK(strstr(contents, "command = \"true # Captured\"") ==
			NULL);
		CHECK(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 0);
		CHECK(attributes(parent_of(windows[0])).x != 1);
	}

	file = fopen(session_path, "w");
	CHECK(file != NULL);
	fputs("window {\n", file);
	CHECK(fclose(file) == 0);
	shortcut(XK_F11, ControlMask | ShiftMask);
	WAIT(log_contains("invalid session"));
	file = fopen(session_path, "r");
	CHECK(file != NULL);
	length = fread(contents, 1, sizeof(contents) - 1, file);
	contents[length] = '\0';
	CHECK(fclose(file) == 0);
	CHECK(strcmp(contents, "window {\n") == 0);

	for (i = 0; i < 4; i++)
		XDestroyWindow(dpy, windows[i]);
	XFlush(dpy);
	WAIT(count_clients() == 0);
	stop_wm();
}

void
test_restore_child_exit(void)
{
	FILE *session = NULL;

	write_config("tray_battery = false\ntray_wireguard = false\n"
		"tray_clock = false\n");
	session = fopen(session_path, "w");
	CHECK(session != NULL);
	fputs("window {\nclass = \"OwmTest\"\ninstance = \"failed\"\n"
	"command = \"sleep 0.1; exit 7\"\n"
	"x = 40\ny = 60\nwidth = 324\nheight = 244\n}\n",
		session);
	CHECK(fclose(session) == 0);
	start_wm();
	/* Reap promptly even with no X events and a distant restore timeout. */
	WAIT(log_contains(
		"restore process for OwmTest/failed exited with status 7"));
	stop_wm();
}
