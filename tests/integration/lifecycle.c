#include "cases.h"
#include "harness.h"

static void	check_resize_handles(Window);

static void
check_resize_handles(Window window)
{
	Window frame;
	size_t i;
	static const int directions[][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1},
		{-1, -1}, {1, -1}, {-1, 1}, {1, 1}};

	frame = parent_of(window);
	for (i = 0; i < sizeof(directions) / sizeof(*directions); i++) {
		XWindowAttributes a, before;
		int dx, dy, x, y;

		a = attributes(frame);
		before = attributes(window);
		dx = directions[i][0] * 8;
		dy = directions[i][1] * 10;
		x = a.x + (dx < 0 ? 1 : dx > 0 ? a.width - 2 : a.width / 2);
		y = a.y + (dy < 0 ? 1 : dy > 0 ? a.height - 2 : a.height / 2);

		drag(x, y, dx, dy);
		CHECK(attributes(window).width == before.width + abs(dx));
		CHECK(attributes(window).height == before.height + abs(dy));
		CHECK((attributes(window).width - 4) % 8 == 0 &&
			(attributes(window).height - 4) % 10 == 0);
	}
}

void
test_lifecycle(void)
{
	unsigned long n, *order = NULL;
	Window dialog, existing, focus, kernel, kframe, popup, qframe, quake;
	int command_count, revert;
	char **command = NULL;
	const char *new_title = NULL;

	existing = make_window("existing", "Adopted before startup", 0);

	XMapWindow(dpy, existing);
	XSync(dpy, False);
	start_wm();
	WAIT(count_clients() == 1);
	CHECK(parent_of(existing) != root);
	CHECK(wait_exit(spawn("./owm")) == 1);

	quake = make_window("quake", "Quake 'console'\nwith escapes", 0);
	kernel = make_window("kernel", "Kernel messages", 0);
	popup = make_window("popup", "Override redirect", 1);

	XMapWindow(dpy, quake);
	XMapWindow(dpy, kernel);
	XMapWindow(dpy, popup);
	XFlush(dpy);
	WAIT(count_clients() == 3);
	CHECK(parent_of(popup) == root);
	XDestroyWindow(dpy, popup);
	XDestroyWindow(dpy, existing);
	XFlush(dpy);
	WAIT(count_clients() == 2);

	qframe = parent_of(quake);
	kframe = parent_of(kernel);

	CHECK(qframe != root && kframe != root);
	XMoveResizeWindow(dpy, quake, 80, 80, 404, 304);
	XMoveResizeWindow(dpy, kernel, 600, 100, 324, 244);
	XFlush(dpy);
	WAIT(attributes(qframe).x == 80 && attributes(kframe).x == 600);
	click(110, 130);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == quake);

	XGetInputFocus(dpy, &focus, &revert);
	CHECK(focus == quake);
	CHECK(received_protocol(quake, "WM_TAKE_FOCUS"));
	drag(140, 94, 91, 37);
	CHECK(attributes(qframe).x == 171 && attributes(qframe).y == 117);

	check_resize_handles(quake);

	XResizeWindow(dpy, quake, 40, 30);
	XFlush(dpy);
	WAIT(attributes(quake).width == 84 && attributes(quake).height == 64);
	XMoveResizeWindow(dpy, quake, 120, 180, 404, 304);
	XFlush(dpy);
	WAIT(attributes(quake).width == 404);
	caption(quake, CAPTION_MAXIMIZE);
	WAIT(state(quake, "_NET_WM_STATE_MAXIMIZED_VERT"));
	CHECK(attributes(qframe).x == 0 && attributes(qframe).y == 0);
	CHECK(attributes(qframe).height <= 772 &&
		attributes(qframe).width <= 1280);
	caption(quake, CAPTION_MAXIMIZE);
	WAIT(!state(quake, "_NET_WM_STATE_MAXIMIZED_VERT"));
	CHECK(attributes(qframe).x == 120 && attributes(qframe).y == 180 &&
		attributes(quake).width == 404);
	caption(quake, CAPTION_MINIMIZE);
	WAIT(state(quake, "_NET_WM_STATE_HIDDEN"));
	CHECK(attributes(qframe).map_state == IsUnmapped &&
		count_clients() == 2);
	click(170, 786);
	WAIT(!state(quake, "_NET_WM_STATE_HIDDEN"));
	CHECK(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == quake);
	click(170, 786);
	WAIT(state(quake, "_NET_WM_STATE_HIDDEN"));
	CHECK(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == kernel);
	click(170, 786);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == quake);
	click(350, 786);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == kernel);
	click(170, 786);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == quake);

	new_title = "Updated UTF-8 title: caf\303\251";

	XChangeProperty(dpy, quake, atom("_NET_WM_NAME"), atom("UTF8_STRING"),
		8, PropModeReplace, (const unsigned char *)new_title,
		(int)strlen(new_title));

	dialog = make_window("dialog", "Transient dialog", 0);

	XSetTransientForHint(dpy, dialog, kernel);
	XMapWindow(dpy, dialog);
	XFlush(dpy);
	WAIT(count_clients() == 3);
	CHECK(parent_of(dialog) != root);
	caption(dialog, CAPTION_MINIMIZE);
	WAIT(state(dialog, "_NET_WM_STATE_HIDDEN") &&
		state(kernel, "_NET_WM_STATE_HIDDEN"));
	click(350, 786);
	WAIT(!state(dialog, "_NET_WM_STATE_HIDDEN") &&
		!state(kernel, "_NET_WM_STATE_HIDDEN"));
	CHECK(attributes(parent_of(dialog)).map_state == IsViewable);
	message(dialog, "_NET_ACTIVE_WINDOW", 1, CurrentTime, 0);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == dialog);
	close_from_menu(dialog);
	CHECK(received_protocol(dialog, "WM_DELETE_WINDOW"));
	XUnmapWindow(dpy, dialog);
	XFlush(dpy);
	WAIT(count_clients() == 2);
	CHECK(parent_of(dialog) == root);
	XDestroyWindow(dpy, dialog);
	XFlush(dpy);
	message(kernel, "_NET_WM_STATE", 1,
		(long)atom("_NET_WM_STATE_MAXIMIZED_VERT"),
		(long)atom("_NET_WM_STATE_MAXIMIZED_HORZ"));
	WAIT(state(kernel, "_NET_WM_STATE_MAXIMIZED_HORZ"));
	message(quake, "WM_CHANGE_STATE", IconicState, 0, 0);
	WAIT(state(quake, "_NET_WM_STATE_HIDDEN"));
	save_session();
	screenshot("desktop");

	stop_wm();
	CHECK(parent_of(quake) == root && parent_of(kernel) == root);
	CHECK(attributes(kernel).map_state == IsViewable &&
		attributes(quake).map_state == IsUnmapped);
	start_wm();
	WAIT(count_clients() == 2);
	pause_ms(300);
	CHECK(count_clients() == 2 && find_instance("quake") == quake &&
		find_instance("kernel") == kernel);
	CHECK(state(quake, "_NET_WM_STATE_HIDDEN") &&
		state(kernel, "_NET_WM_STATE_MAXIMIZED_HORZ"));
	stop_wm();
	XDestroyWindow(dpy, quake);
	XDestroyWindow(dpy, kernel);
	XSync(dpy, False);
	start_wm();
	WAIT(count_clients() == 2);
	quake = find_instance("quake");
	kernel = find_instance("kernel");
	CHECK(quake && kernel);
	CHECK(state(quake, "_NET_WM_STATE_HIDDEN") &&
		state(kernel, "_NET_WM_STATE_MAXIMIZED_HORZ"));
	CHECK(attributes(parent_of(quake)).x == 120 &&
		attributes(parent_of(quake)).y == 180);
	message(kernel, "_NET_WM_STATE", 0,
		(long)atom("_NET_WM_STATE_MAXIMIZED_VERT"),
		(long)atom("_NET_WM_STATE_MAXIMIZED_HORZ"));
	WAIT(!state(kernel, "_NET_WM_STATE_MAXIMIZED_HORZ"));
	CHECK(attributes(parent_of(kernel)).x == 600 &&
		attributes(parent_of(kernel)).y == 100);

	order = property(root, "_NET_CLIENT_LIST", XA_WINDOW, &n);

	CHECK(n == 2 && order[0] == quake && order[1] == kernel);
	XFree(order);

	CHECK(XGetCommand(dpy, quake, &command, &command_count));
	CHECK(command_count == 5 &&
		!strcmp(command[4], "literal ' \" $HOME ; \\ newline\nvalue"));
	XFreeStringList(command);
	message(quake, "_NET_CLOSE_WINDOW", 0, 0, 0);
	message(kernel, "_NET_CLOSE_WINDOW", 0, 0, 0);
	WAIT(count_clients() == 0);
	stop_wm();
}

void
test_signal_shutdown(void)
{
	Window survivor;

	write_config("restore_session = false\ntray_battery = false\n"
		"tray_wireguard = false\ntray_clock = false\n");
	start_wm();
	survivor = make_window("survivor", "Signal cleanup", 0);

	XMapWindow(dpy, survivor);
	XFlush(dpy);
	WAIT(count_clients() == 1);
	/* With no timers or X events, shutdown must wake an indefinite wait. */
	pause_ms(100);
	CHECK(kill(wm_pid, SIGTERM) == 0);
	CHECK(wait_exit(wm_pid) == 0);
	wm_pid = 0;
	CHECK(parent_of(survivor) == root &&
		attributes(survivor).map_state == IsViewable);
	XDestroyWindow(dpy, survivor);
	XSync(dpy, False);
}

void
test_restart(void)
{
	struct stat after, saved;
	XImage *pixel = NULL;
	XWindowAttributes before;
	Window app, closed, maximized, minimized;

	write_config("restore_session = false\n");
	start_wm();

	app = make_window("restart-app", "Keep open", 0);
	closed = make_window("restart-closed", "Keep closed", 0);

	XMapWindow(dpy, app);
	XMapWindow(dpy, closed);
	XFlush(dpy);
	WAIT(count_clients() == 2);
	save_session();

	CHECK(stat(session_path, &saved) == 0);
	XDestroyWindow(dpy, closed);
	XMoveResizeWindow(dpy, app, 300, 200, 324, 244);
	XFlush(dpy);
	WAIT(count_clients() == 1 && attributes(parent_of(app)).x == 300);
	message(app, "_NET_WM_DESKTOP", 1, 0, 0);
	message(root, "_NET_CURRENT_DESKTOP", 1, CurrentTime, 0);
	WAIT(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 1);

	minimized = make_window("restart-minimized", "Minimized", 0);
	maximized = make_window("restart-maximized", "Maximized", 0);

	XMapWindow(dpy, minimized);
	XMapWindow(dpy, maximized);
	XFlush(dpy);
	WAIT(count_clients() == 3);
	message(minimized, "WM_CHANGE_STATE", IconicState, 0, 0);
	message(maximized, "_NET_WM_STATE", 1,
		atom("_NET_WM_STATE_MAXIMIZED_HORZ"),
		atom("_NET_WM_STATE_MAXIMIZED_VERT"));
	message(maximized, "_NET_WM_DESKTOP", 0, 0, 0);
	WAIT(state(minimized, "_NET_WM_STATE_HIDDEN") &&
		state(maximized, "_NET_WM_STATE_MAXIMIZED_VERT"));
	before = attributes(parent_of(app));

	write_config("restore_session = true\n"
		"background_color = \"#234567\"\n"
		"taskbar_height = 44\n"
		"desktops = 2\n"
		"bind = \"Super+r session_restart\"\n");
	right_click(1100, 600);
	WAIT(desktop_menu());
	shortcut(XK_Home, 0);
	shortcut(XK_Down, 0);
	shortcut(XK_Down, 0);
	shortcut(XK_Return, 0);
	/* X11 may reuse frame IDs after the WM reconnects. */
	WAIT(parent_of(app) != root && count_clients() == 3 &&
		attributes(taskbar_window()).height == 44);
	CHECK(kill(wm_pid, 0) == 0);
	CHECK(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 1);
	CHECK(attributes(parent_of(app)).x == before.x &&
		attributes(parent_of(app)).y == before.y &&
		attributes(parent_of(app)).width == before.width &&
		attributes(parent_of(app)).height == before.height);
	CHECK(state(minimized, "_NET_WM_STATE_HIDDEN"));
	CHECK(state(maximized, "_NET_WM_STATE_MAXIMIZED_VERT"));
	CHECK(!find_instance("restart-closed"));

	pixel = XGetImage(dpy, root, 1100, 600, 1, 1, AllPlanes, ZPixmap);

	CHECK(pixel && XGetPixel(pixel, 0, 0) == 0x234567);
	XDestroyImage(pixel);

	/* A second restart exercises the new binding and clamps desktops. */
	write_config("restore_session = true\ndesktops = 1\n");
	shortcut(XK_r, Mod4Mask);
	WAIT(value(root, "_NET_NUMBER_OF_DESKTOPS", XA_CARDINAL) == 1 &&
		count_clients() == 3);
	CHECK(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 0);
	CHECK(value(app, "_NET_WM_DESKTOP", XA_CARDINAL) == 0);
	CHECK(stat(session_path, &after) == 0);
	CHECK(saved.st_size == after.st_size &&
		saved.st_mtim.tv_sec == after.st_mtim.tv_sec &&
		saved.st_mtim.tv_nsec == after.st_mtim.tv_nsec);
	XDestroyWindow(dpy, app);
	XDestroyWindow(dpy, minimized);
	XDestroyWindow(dpy, maximized);
	XFlush(dpy);
	WAIT(count_clients() == 0);
	stop_wm();
	CHECK(unlink(session_path) == 0);
}
