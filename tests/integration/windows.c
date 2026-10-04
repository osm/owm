#include "cases.h"
#include "harness.h"

void
test_maximize_transitions(void)
{
	XWindowAttributes original, frame, maximized;
	unsigned long *saved = NULL, count;
	Window app;
	Atom horizontal, vertical, fullscreen;

	start_wm();
	app = make_window("maximize-transitions", "Maximize transitions", 0);
	XMapWindow(dpy, app);
	XFlush(dpy);
	WAIT(count_clients() == 1);
	original = attributes(app);
	frame = attributes(parent_of(app));
	horizontal = atom("_NET_WM_STATE_MAXIMIZED_HORZ");
	vertical = atom("_NET_WM_STATE_MAXIMIZED_VERT");
	fullscreen = atom("_NET_WM_STATE_FULLSCREEN");
	message(app, "_NET_WM_STATE", 1, vertical, 0);
	WAIT(state(app, "_NET_WM_STATE_MAXIMIZED_VERT"));
	XMoveResizeWindow(dpy, app, 200, 111, 500, 354);
	XFlush(dpy);
	WAIT(attributes(app).width == 500 &&
		attributes(parent_of(app)).x == 200);
	saved = property(app, "_OWM_RESTORE_GEOMETRY", XA_CARDINAL, &count);
	CHECK(saved && count == 4 && saved[0] == 200 && saved[2] == 500);
	XFree(saved);
	message(app, "_NET_WM_STATE", 1, fullscreen, 0);
	check_fullscreen(app);
	message(app, "_NET_WM_STATE", 0, fullscreen, 0);
	WAIT(!state(app, "_NET_WM_STATE_FULLSCREEN"));
	CHECK(attributes(app).width == 500 &&
		attributes(parent_of(app)).x == 200);
	message(app, "_NET_WM_STATE", 0, vertical, 0);
	WAIT(!state(app, "_NET_WM_STATE_MAXIMIZED_VERT"));
	CHECK(attributes(app).width == 500 &&
		attributes(app).height == original.height);
	CHECK(attributes(parent_of(app)).x == 200 &&
		attributes(parent_of(app)).y == frame.y);

	message(app, "_NET_WM_STATE", 1, horizontal, 0);
	WAIT(state(app, "_NET_WM_STATE_MAXIMIZED_HORZ"));
	XMoveResizeWindow(dpy, app, 111, 160, 404, 354);
	XFlush(dpy);
	WAIT(attributes(app).height == 354 &&
		attributes(parent_of(app)).y == 160);
	message(app, "_NET_WM_STATE", 0, horizontal, 0);
	WAIT(!state(app, "_NET_WM_STATE_MAXIMIZED_HORZ"));
	CHECK(attributes(app).width == 500 && attributes(app).height == 354);
	CHECK(attributes(parent_of(app)).x == 200 &&
		attributes(parent_of(app)).y == 160);

	frame = attributes(parent_of(app));
	window_move_pointer(
		frame.x + frame.width - 2, frame.y + frame.height / 2);
	fake_button(dpy, Button1, True, 0);
	XFlush(dpy);
	pause_ms(50);
	shortcut(XK_m, Mod4Mask | ShiftMask);
	WAIT(state(app, "_NET_WM_STATE_MAXIMIZED_VERT"));
	maximized = attributes(parent_of(app));
	window_move_pointer(
		frame.x + frame.width + 50, frame.y + frame.height / 2);
	fake_button(dpy, Button1, False, 0);
	XFlush(dpy);
	pause_ms(100);
	CHECK(state(app, "_NET_WM_STATE_MAXIMIZED_HORZ"));
	CHECK(attributes(parent_of(app)).x == maximized.x &&
		attributes(parent_of(app)).y == maximized.y);
	CHECK(attributes(parent_of(app)).width == maximized.width &&
		attributes(parent_of(app)).height == maximized.height);
	/* Titlebar dragging must still restore and move a maximized window. */
	drag(maximized.width / 2, 14, 100, 100);
	CHECK(!state(app, "_NET_WM_STATE_MAXIMIZED_HORZ") &&
		!state(app, "_NET_WM_STATE_MAXIMIZED_VERT"));
	CHECK(attributes(app).width == 500 && attributes(app).height == 354);
	XDestroyWindow(dpy, app);
	XFlush(dpy);
	WAIT(count_clients() == 0);
	stop_wm();
}

void
test_fullscreen(void)
{
	XWindowAttributes maximized, original, original_frame;
	Atom full;
	unsigned long i;
	unsigned long n, *supported = NULL;
	Window a, b, dialog;
	int advertised;

	write_config("restore_session = true\nbind = \"Ctrl+F11 "
		"window_fullscreen\"\n");

	CHECK(access(session_path, F_OK) < 0 && errno == ENOENT);
	start_wm();
	message(root, "_NET_CURRENT_DESKTOP", 0, CurrentTime, 0);
	WAIT(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 0);

	supported = property(root, "_NET_SUPPORTED", XA_ATOM, &n);
	advertised = 0;

	for (i = 0; supported != NULL && i < n; i++)
		advertised |= supported[i] == atom("_NET_WM_STATE_FULLSCREEN");

	CHECK(advertised);
	XFree(supported);

	a = make_window("fullscreen", "Fullscreen test", 0);

	XMapWindow(dpy, a);
	XFlush(dpy);
	WAIT(count_clients() == 1);

	original = attributes(a);
	original_frame = attributes(parent_of(a));
	full = atom("_NET_WM_STATE_FULLSCREEN");

	message(a, "_NET_WM_STATE", 1, full, 0);
	check_fullscreen(a);
	CHECK(stack_position(parent_of(a)) > stack_position(taskbar_window()));

	/*
	 * Duplicate add and configure requests cannot overwrite the restore
	 * geometry.
	 */
	message(a, "_NET_WM_STATE", 1, full, 0);
	XMoveResizeWindow(dpy, a, 80, 90, 400, 300);
	XFlush(dpy);
	pause_ms(100);
	check_fullscreen(a);

	b = make_window("normal", "Normal app", 0);

	XMapWindow(dpy, b);
	XFlush(dpy);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == b);
	CHECK(stack_position(parent_of(b)) > stack_position(parent_of(a)));
	CHECK(stack_position(taskbar_window()) > stack_position(parent_of(a)));
	shortcut(XK_Tab, Mod1Mask);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == a);
	CHECK(stack_position(parent_of(a)) > stack_position(taskbar_window()));

	dialog = make_window("full-dialog", "Game dialog", 0);

	XSetTransientForHint(dpy, dialog, a);
	XMapWindow(dpy, dialog);
	XFlush(dpy);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == dialog);
	CHECK(stack_position(parent_of(dialog)) > stack_position(parent_of(a)));
	CHECK(stack_position(parent_of(a)) > stack_position(taskbar_window()));
	XDestroyWindow(dpy, dialog);
	XFlush(dpy);
	WAIT(count_clients() == 2);
	message(root, "_NET_CURRENT_DESKTOP", 1, CurrentTime, 0);
	WAIT(attributes(parent_of(a)).map_state == IsUnmapped);

	/* SDL's minimization on focus loss must not persist across desktops. */
	message(a, "WM_CHANGE_STATE", IconicState, 0, 0);

	/* SDL can also leave fullscreen before requesting minimization. */
	message(a, "_NET_WM_STATE", 0, full, 0);
	WAIT(!state(a, "_NET_WM_STATE_FULLSCREEN"));
	message(a, "WM_CHANGE_STATE", IconicState, 0, 0);
	WAIT(state(a, "_NET_WM_STATE_FULLSCREEN"));
	message(root, "_NET_CURRENT_DESKTOP", 0, CurrentTime, 0);
	WAIT(attributes(parent_of(a)).map_state == IsViewable);
	CHECK(!state(a, "_NET_WM_STATE_HIDDEN"));
	message(a, "_NET_ACTIVE_WINDOW", 1, CurrentTime, 0);
	message(a, "WM_CHANGE_STATE", IconicState, 0, 0);
	WAIT(state(a, "_NET_WM_STATE_HIDDEN"));
	message(root, "_NET_CURRENT_DESKTOP", 1, CurrentTime, 0);
	WAIT(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 1);
	message(root, "_NET_CURRENT_DESKTOP", 0, CurrentTime, 0);
	WAIT(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 0);
	CHECK(state(a, "_NET_WM_STATE_HIDDEN"));
	message(a, "_NET_ACTIVE_WINDOW", 1, CurrentTime, 0);
	WAIT(!state(a, "_NET_WM_STATE_HIDDEN"));
	check_fullscreen(a);

	/* Toggle in both atom slots only once. */
	message(a, "_NET_WM_STATE", 2, full, full);
	WAIT(!state(a, "_NET_WM_STATE_FULLSCREEN"));
	CHECK(attributes(a).width == original.width &&
		attributes(a).height == original.height);
	CHECK(attributes(parent_of(a)).x == original_frame.x &&
		attributes(parent_of(a)).y == original_frame.y);
	CHECK(value(a, "_NET_FRAME_EXTENTS", XA_CARDINAL) == FRAME_BORDER);
	shortcut(XK_F11, ControlMask);
	check_fullscreen(a);
	shortcut(XK_F11, ControlMask);
	WAIT(!state(a, "_NET_WM_STATE_FULLSCREEN"));
	message(a, "_NET_WM_STATE", 1, atom("_NET_WM_STATE_MAXIMIZED_HORZ"),
		atom("_NET_WM_STATE_MAXIMIZED_VERT"));
	WAIT(state(a, "_NET_WM_STATE_MAXIMIZED_VERT"));

	maximized = attributes(a);

	message(a, "_NET_WM_STATE", 1, full, 0);
	check_fullscreen(a);
	message(a, "_NET_WM_STATE", 0, full, 0);
	WAIT(!state(a, "_NET_WM_STATE_FULLSCREEN"));
	CHECK(attributes(a).width == maximized.width &&
		attributes(a).height == maximized.height);
	message(a, "_NET_WM_STATE", 1, full, 0);
	check_fullscreen(a);

	/*
	 * Changing maximization while fullscreen affects only the eventual
	 * restore.
	 */
	message(a, "_NET_WM_STATE", 0, atom("_NET_WM_STATE_MAXIMIZED_HORZ"),
		atom("_NET_WM_STATE_MAXIMIZED_VERT"));
	WAIT(!state(a, "_NET_WM_STATE_MAXIMIZED_VERT"));
	check_fullscreen(a);
	XDestroyWindow(dpy, b);
	XFlush(dpy);
	WAIT(count_clients() == 1);
	save_session();
	stop_wm();
	CHECK(attributes(a).x == 0 && attributes(a).y == 0);
	start_wm();
	check_fullscreen(a);
	message(a, "_NET_WM_STATE", 0, full, 0);
	WAIT(!state(a, "_NET_WM_STATE_FULLSCREEN"));
	CHECK(attributes(a).width == original.width &&
		attributes(a).height == original.height);
	CHECK(attributes(parent_of(a)).x == original_frame.x &&
		attributes(parent_of(a)).y == original_frame.y);
	XDestroyWindow(dpy, a);
	XFlush(dpy);
	WAIT(count_clients() == 0);
	stop_wm();
	start_wm();
	WAIT(find_instance("fullscreen"));
	a = find_instance("fullscreen");
	check_fullscreen(a);
	message(a, "_NET_WM_STATE", 0, full, 0);
	WAIT(!state(a, "_NET_WM_STATE_FULLSCREEN"));
	CHECK(attributes(a).width == original.width &&
		attributes(a).height == original.height);
	message(a, "_NET_CLOSE_WINDOW", CurrentTime, 0, 0);
	WAIT(count_clients() == 0);
	CHECK(unlink(session_path) == 0);

	/* Applications can request fullscreen before their first map. */
	a = make_window("initial-fullscreen", "Starts fullscreen", 0);
	XChangeProperty(dpy, a, atom("_NET_WM_STATE"), XA_ATOM, 32,
		PropModeReplace, (unsigned char *)&full, 1);
	message(a, "_NET_REQUEST_FRAME_EXTENTS", 0, 0, 0);
	WAIT(value(a, "_NET_FRAME_EXTENTS", XA_CARDINAL) == 0);
	XMapWindow(dpy, a);
	XFlush(dpy);
	WAIT(count_clients() == 1);
	check_fullscreen(a);
	message(a, "_NET_WM_STATE", 0, full, 0);
	WAIT(!state(a, "_NET_WM_STATE_FULLSCREEN"));
	CHECK(attributes(a).width == original.width &&
		attributes(a).height == original.height);
	XDestroyWindow(dpy, a);
	XFlush(dpy);
	WAIT(count_clients() == 0);
	stop_wm();
}

void
test_protocol_properties(void)
{
	Window app;
	Atom states[2];
	unsigned long *extents = NULL, n;

	start_wm();
	app = make_window("protocol", "Protocol property ownership", 0);
	message(app, "_NET_REQUEST_FRAME_EXTENTS", 0, 0, 0);
	WAIT(value(app, "_NET_FRAME_EXTENTS", XA_CARDINAL) == FRAME_BORDER);
	extents = property(app, "_NET_FRAME_EXTENTS", XA_CARDINAL, &n);
	CHECK(n == 4 && extents[1] == FRAME_BORDER &&
		extents[2] > FRAME_BORDER && extents[3] == FRAME_BORDER);
	XFree(extents);

	states[0] = atom("_NET_WM_STATE_DEMANDS_ATTENTION");
	states[1] = atom("_NET_WM_STATE_FULLSCREEN");
	XChangeProperty(dpy, app, atom("_NET_WM_STATE"), XA_ATOM, 32,
		PropModeReplace, (unsigned char *)states, 2);
	message(app, "_NET_REQUEST_FRAME_EXTENTS", 0, 0, 0);
	WAIT(value(app, "_NET_FRAME_EXTENTS", XA_CARDINAL) == 0);
	XMapWindow(dpy, app);
	XFlush(dpy);
	WAIT(count_clients() == 1);
	check_fullscreen(app);
	CHECK(state(app, "_NET_WM_STATE_DEMANDS_ATTENTION"));

	message(app, "_NET_WM_STATE", 1,
		(long)atom("_NET_WM_STATE_SKIP_TASKBAR"),
		(long)atom("_NET_WM_STATE_MAXIMIZED_HORZ"));
	WAIT(state(app, "_NET_WM_STATE_SKIP_TASKBAR") &&
		state(app, "_NET_WM_STATE_MAXIMIZED_HORZ"));
	CHECK(state(app, "_NET_WM_STATE_FULLSCREEN") &&
		state(app, "_NET_WM_STATE_DEMANDS_ATTENTION"));
	message(app, "_NET_WM_STATE", 0, (long)atom("_NET_WM_STATE_FULLSCREEN"),
		(long)atom("_NET_WM_STATE_SKIP_TASKBAR"));
	WAIT(!state(app, "_NET_WM_STATE_FULLSCREEN") &&
		!state(app, "_NET_WM_STATE_SKIP_TASKBAR"));
	CHECK(state(app, "_NET_WM_STATE_MAXIMIZED_HORZ") &&
		state(app, "_NET_WM_STATE_DEMANDS_ATTENTION"));
	CHECK(value(app, "_NET_FRAME_EXTENTS", XA_CARDINAL) == FRAME_BORDER);
	XDestroyWindow(dpy, app);
	XFlush(dpy);
	WAIT(count_clients() == 0);
	stop_wm();
}

void
test_sdl(void)
{
	if (access("./build/tests/sdl-fullscreen", X_OK) != 0) {
		fputs("SDL2 helper is not built (requires libsdl2-dev)\n",
			stderr);
		exit(TEST_SKIP);
	}
	start_wm();
	message(root, "_NET_CURRENT_DESKTOP", 0, CurrentTime, 0);
	WAIT(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 0);
	CHECK(wait_exit(spawn("./build/tests/sdl-fullscreen")) == 0);
	WAIT(count_clients() == 0);
	stop_wm();
}

void
test_titlebar_clicks(void)
{
	Window a;
	XWindowAttributes before;

	start_wm();
	a = make_window("titlebar-clicks", "Titlebar clicks", 0);
	XMapWindow(dpy, a);
	XFlush(dpy);
	WAIT(count_clients() == 1);
	XMoveWindow(dpy, a, 80, 100);
	XFlush(dpy);
	WAIT(attributes(parent_of(a)).x == 80);

	before = attributes(a);

	click(140, 114);
	click(140, 114);
	WAIT(state(a, "_NET_WM_STATE_MAXIMIZED_HORZ"));
	CHECK(attributes(parent_of(a)).height <= 772);
	click(60, 14);
	click(60, 14);
	WAIT(!state(a, "_NET_WM_STATE_MAXIMIZED_HORZ"));
	CHECK(attributes(parent_of(a)).x == 80 &&
		attributes(parent_of(a)).y == 100);
	CHECK(attributes(a).width == before.width &&
		attributes(a).height == before.height);
	click(140, 114);
	pause_ms(350);
	click(140, 114);
	CHECK(!state(a, "_NET_WM_STATE_MAXIMIZED_HORZ"));
	pause_ms(350);
	drag(140, 114, 30, 20);
	CHECK(attributes(parent_of(a)).x == 110 &&
		!state(a, "_NET_WM_STATE_MAXIMIZED_HORZ"));

	XDestroyWindow(dpy, a);
	XFlush(dpy);
	WAIT(count_clients() == 0);
	stop_wm();
}

void
test_metadata_updates(void)
{
	Window app, other;
	XSizeHints hints = {0};
	Atom protocols[2];

	start_wm();
	app = make_window("metadata", "Changing hints", 0);
	other = make_window("metadata-other", "Other window", 0);
	protocols[0] = atom("WM_DELETE_WINDOW");
	XSetWMProtocols(dpy, app, protocols, 1);
	XMapWindow(dpy, app);
	XMapWindow(dpy, other);
	XFlush(dpy);
	WAIT(count_clients() == 2);

	/* Live size hints must take effect without remapping the window. */
	hints.flags = PMinSize;
	hints.min_width = 400;
	hints.min_height = 300;
	XSetWMNormalHints(dpy, app, &hints);
	XResizeWindow(dpy, app, 100, 100);
	XFlush(dpy);
	WAIT(attributes(app).width == 400 && attributes(app).height == 300);
	XDeleteProperty(dpy, app, atom("WM_NORMAL_HINTS"));
	XResizeWindow(dpy, app, 100, 100);
	XFlush(dpy);
	WAIT(attributes(app).width == 100 && attributes(app).height == 100);

	/* Protocol changes must also be picked up after management. */
	protocols[1] = atom("WM_TAKE_FOCUS");
	XSetWMProtocols(dpy, app, protocols, 2);
	message(app, "_NET_ACTIVE_WINDOW", 1, CurrentTime, 0);
	WAIT(received_protocol(app, "WM_TAKE_FOCUS"));
	/*
	 * Repeated activation keeps the protocol but leaves the property alone.
	 */
	XSelectInput(dpy, root, PropertyChangeMask);
	XSync(dpy, False);
	{
		XEvent event;

		while (XCheckWindowEvent(dpy, root, PropertyChangeMask, &event))
			;
		message(app, "_NET_ACTIVE_WINDOW", 1, CurrentTime, 0);
		WAIT(XCheckTypedWindowEvent(dpy, app, ClientMessage, &event));
		CHECK(event.xclient.message_type == atom("WM_PROTOCOLS") &&
			(Atom)event.xclient.data.l[0] == atom("WM_TAKE_FOCUS"));
		XSync(dpy, False);
		while (XCheckWindowEvent(dpy, root, PropertyChangeMask, &event))
			CHECK(event.xproperty.atom !=
				atom("_NET_ACTIVE_WINDOW"));
	}
	XSetWMProtocols(dpy, app, protocols, 1);
	message(other, "_NET_ACTIVE_WINDOW", 1, CurrentTime, 0);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == other);
	message(app, "_NET_ACTIVE_WINDOW", 1, CurrentTime, 0);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == app);
	CHECK(!received_protocol(app, "WM_TAKE_FOCUS"));

	XDestroyWindow(dpy, app);
	XDestroyWindow(dpy, other);
	XFlush(dpy);
	WAIT(count_clients() == 0);
	stop_wm();
}
