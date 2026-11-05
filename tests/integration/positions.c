#include "cases.h"
#include "harness.h"

static Window	open_app(const char *);
static void	move_app(Window, int, int);
static void	destroy_app(Window);
static void	position_menu(int);

static Window
open_app(const char *instance)
{
	Window app;
	XSizeHints hints = {0};

	app = make_window(instance, "A different title on every launch", 0);
	/* Remembered placement overrides the app's initial position hint. */
	hints.flags = PPosition;
	hints.x = 40;
	hints.y = 60;
	XSetWMNormalHints(dpy, app, &hints);
	XMapWindow(dpy, app);
	XFlush(dpy);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == app);

	return app;
}

static void
move_app(Window app, int x, int y)
{
	XMoveWindow(dpy, app, x, y);
	XFlush(dpy);
	WAIT(attributes(parent_of(app)).x == x &&
		attributes(parent_of(app)).y == y);
}

static void
destroy_app(Window app)
{
	XDestroyWindow(dpy, app);
	XFlush(dpy);
	WAIT(count_clients() == 0);
}

static void
position_menu(int forget)
{
	int i;

	shortcut(XK_w, Mod4Mask);
	WAIT(named_popup("owm window menu"));
	shortcut(XK_Home, 0);
	for (i = 0; i < 3 + forget; i++)
		shortcut(XK_Down, 0);
	screenshot(forget ? "forget-position" : "remember-position");
	shortcut(XK_Return, 0);
	WAIT(!named_popup("owm window menu"));
}

void
test_remember_position(void)
{
	Window app, dialog;
	XWindowAttributes a;

	write_config("restore_session = false\n");
	start_wm();
	app = open_app("remembered");
	XStoreName(dpy, app, "Original title");
	move_app(app, 321, 123);
	XResizeWindow(dpy, app, 500, 300);
	XFlush(dpy);
	WAIT(attributes(app).width == 500);
	position_menu(0);
	/*
	 * Later moves must not silently change the explicitly saved position.
	 */
	move_app(app, 50, 70);
	destroy_app(app);
	app = open_app("remembered");
	a = attributes(parent_of(app));
	CHECK(a.x == 321 && a.y == 123);
	CHECK(attributes(app).width == 324);

	/* Same identity on a transient must still use dialog placement. */
	dialog = make_window("remembered", "Dialog", 0);
	XSetTransientForHint(dpy, dialog, app);
	XResizeWindow(dpy, dialog, 164, 104);
	XMapWindow(dpy, dialog);
	XFlush(dpy);
	WAIT(count_clients() == 2);
	CHECK(attributes(parent_of(dialog)).x != 321);
	XDestroyWindow(dpy, dialog);
	XFlush(dpy);
	WAIT(count_clients() == 1);

	/* Capture normal placement when saving a maximized window. */
	move_app(app, 456, 234);
	shortcut(XK_m, Mod4Mask | ShiftMask);
	WAIT(state(app, "_NET_WM_STATE_MAXIMIZED_VERT"));
	position_menu(0);
	destroy_app(app);
	stop_wm();
	start_wm();
	app = open_app("remembered");
	a = attributes(parent_of(app));
	CHECK(a.x == 456 && a.y == 234);
	CHECK(!state(app, "_NET_WM_STATE_MAXIMIZED_VERT"));

	/* Adopting an existing window must preserve its current position. */
	move_app(app, 70, 90);
	stop_wm();
	start_wm();
	WAIT(count_clients() == 1);
	a = attributes(parent_of(app));
	CHECK(a.x == 70 && a.y == 90);
	destroy_app(app);
	app = open_app("other-instance");
	a = attributes(parent_of(app));
	CHECK(a.x == 40 && a.y == 60);
	destroy_app(app);
	app = open_app("remembered");
	CHECK(attributes(parent_of(app)).x == 456);
	position_menu(1);
	destroy_app(app);
	stop_wm();
	start_wm();
	app = open_app("remembered");
	a = attributes(parent_of(app));
	CHECK(a.x == 40 && a.y == 60);
	destroy_app(app);
	stop_wm();
}
