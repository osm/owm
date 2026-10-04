#include "harness.h"

Window
make_window(const char *instance, const char *title, int override)
{
	XSetWindowAttributes a = {0};
	XSizeHints hints = {0};
	XClassHint class_hint = {0};
	Atom protocols[2] = {0};
	Window w;
	char *args[5] = {0};

	a.override_redirect = override;
	a.background_pixel = 0xeeeeee;
	a.event_mask = StructureNotifyMask | ButtonPressMask;
	w = XCreateWindow(dpy, root, 40, 60, 324, 244, 0, CopyFromParent,
		InputOutput, CopyFromParent,
		CWOverrideRedirect | CWBackPixel | CWEventMask, &a);
	class_hint.res_name = (char *)instance;
	class_hint.res_class = "OwmTest";

	XSetClassHint(dpy, w, &class_hint);
	XStoreName(dpy, w, title);

	protocols[0] = atom("WM_DELETE_WINDOW");
	protocols[1] = atom("WM_TAKE_FOCUS");

	XSetWMProtocols(dpy, w, protocols, 2);

	hints.flags = PMinSize | PBaseSize | PResizeInc;
	hints.min_width = 84;
	hints.min_height = 64;
	hints.base_width = 4;
	hints.base_height = 4;
	hints.width_inc = 8;
	hints.height_inc = 10;

	XSetWMNormalHints(dpy, w, &hints);

	args[0] = fixture_path;
	args[1] = "--fixture";
	args[2] = (char *)instance;
	args[3] = (char *)title;
	args[4] = "literal ' \" $HOME ; \\ newline\nvalue";

	XSetCommand(dpy, w, args, 5);

	return w;
}

int
fixture(int argc, char **argv)
{
	Window w;

	if (argc < 4)
		return 1;

	dpy = XOpenDisplay(NULL);

	if (dpy == NULL)
		return 1;

	root = DefaultRootWindow(dpy);
	CHECK(string_copy(fixture_path, argv[0], sizeof(fixture_path)) <
		sizeof(fixture_path));

	w = make_window(argv[2], argv[3], 0);

	XSetCommand(dpy, w, argv, argc);
	XMapWindow(dpy, w);

	for (;;) {
		XEvent e;

		XNextEvent(dpy, &e);

		if (e.type == ClientMessage &&
			e.xclient.message_type == atom("WM_PROTOCOLS") &&
			(Atom)e.xclient.data.l[0] == atom("WM_DELETE_WINDOW"))
			break;
	}

	XDestroyWindow(dpy, w);
	XCloseDisplay(dpy);

	return 0;
}

pid_t
start_xterm(const char *name, const char *text)
{
	pid_t pid;

	pid = fork();

	CHECK(pid >= 0);

	if (pid == 0) {
		close(ConnectionNumber(dpy));
		execlp("xterm", "xterm", "-name", name, "-title", name,
			"-geometry", "64x18", "-e", "/bin/sh", "-c",
			"printf '%s\\n' \"$1\"; exec sleep 120", "sh", text,
			(char *)NULL);
		_exit(127);
	}

	return pid;
}
