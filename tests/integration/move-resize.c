#include "cases.h"
#include "harness.h"

static void	start_keyboard_move(void);
static void	check_move_steps(Window app, int step);
static void	check_move_fullscreen(Window app, int step);
static void	check_coordinate_entry(Window app, const char *image_name);
static void	check_mouse_move(Window app, int step, int resize_step);
static void	check_move_maximized(Window app);
static void	check_fixed_size(Window app);
static void	check_close_during_move(Window app);

static void
start_keyboard_move(void)
{
	shortcut(XK_w, Mod4Mask);
	WAIT(named_popup("owm window menu"));
	shortcut(XK_Return, 0);
	WAIT(named_popup("owm resize/move"));
	CHECK(!named_popup("owm window menu"));
}

static void
check_move_steps(Window app, int step)
{
	Window frame;
	XWindowAttributes initial, content;

	frame = parent_of(app);
	initial = attributes(frame);
	content = attributes(app);
	/* The left titlebar icon opens the menu at both move steps. */
	click(initial.x + FRAME_BORDER + 10, initial.y + FRAME_BORDER + 10);
	WAIT(named_popup("owm window menu"));
	CHECK(attributes(frame).x == initial.x &&
		attributes(frame).y == initial.y);
	shortcut(XK_Escape, 0);
	WAIT(!named_popup("owm window menu"));
	start_keyboard_move();
	CHECK(count_clients() == 1);
	shortcut(XK_Right, 0);
	shortcut(XK_Down, ShiftMask);
	WAIT(attributes(frame).x == initial.x + step &&
		attributes(frame).y == initial.y + 1);
	shortcut(XK_Right, ControlMask | ShiftMask);
	WAIT(attributes(app).width == content.width + 8);
	shortcut(XK_Down, ControlMask | ShiftMask);
	WAIT(attributes(app).height == content.height + 10);
	shortcut(XK_Left, ControlMask | ShiftMask);
	WAIT(attributes(app).width == content.width);
	screenshot(step == 13 ? "custom-keyboard-move" : "keyboard-move");
	shortcut(XK_Escape, 0);
	WAIT(!named_popup("owm resize/move"));
	CHECK(attributes(frame).x == initial.x &&
		attributes(frame).y == initial.y &&
		attributes(app).width == content.width &&
		attributes(app).height == content.height);
}

static void
check_move_fullscreen(Window app, int step)
{
	Window frame;
	XWindowAttributes initial, content;

	frame = parent_of(app);
	initial = attributes(frame);
	content = attributes(app);
	/*
	 * A fullscreen request cancels the move and releases its grabs.
	 */
	start_keyboard_move();
	shortcut(XK_Right, 0);
	WAIT(attributes(frame).x == initial.x + step);
	message(app, "_NET_WM_STATE", 1, atom("_NET_WM_STATE_FULLSCREEN"), 0);
	WAIT(state(app, "_NET_WM_STATE_FULLSCREEN"));
	WAIT(!named_popup("owm resize/move"));
	check_fullscreen(app);
	CHECK(XGrabKeyboard(dpy, app, False, GrabModeAsync, GrabModeAsync,
		CurrentTime) == GrabSuccess);
	XUngrabKeyboard(dpy, CurrentTime);
	CHECK(XGrabPointer(dpy, app, False, ButtonPressMask, GrabModeAsync,
		GrabModeAsync, None, None, CurrentTime) == GrabSuccess);
	XUngrabPointer(dpy, CurrentTime);
	message(app, "_NET_WM_STATE", 0, atom("_NET_WM_STATE_FULLSCREEN"), 0);
	WAIT(!state(app, "_NET_WM_STATE_FULLSCREEN"));
	CHECK(attributes(frame).x == initial.x &&
		attributes(frame).y == initial.y);

	CHECK(attributes(app).width == content.width &&
		attributes(app).height == content.height);
}

static void
check_coordinate_entry(Window app, const char *image_name)
{
	Window frame;
	XWindowAttributes initial, helper, content;

	frame = parent_of(app);
	initial = attributes(frame);
	content = attributes(app);
	/*
	 * Numeric coordinates work with keyboard and mouse field
	 * selection.
	 */
	start_keyboard_move();
	shortcut(XK_Tab, 0);
	type_command("321");
	shortcut(XK_Tab, 0);
	type_command("123");
	shortcut(XK_Return, 0);
	WAIT(!named_popup("owm resize/move"));
	WAIT(attributes(frame).x == 321 && attributes(frame).y == 123);
	XMoveWindow(dpy, app, initial.x, initial.y);
	XFlush(dpy);
	WAIT(attributes(frame).x == initial.x &&
		attributes(frame).y == initial.y);

	start_keyboard_move();
	helper = attributes(named_popup("owm resize/move"));
	click(helper.x + 40, helper.y + 6 + (helper.height - 12) / 4 + 8);
	type_command("999999999999");
	shortcut(XK_Return, 0);
	CHECK(named_popup("owm resize/move"));
	CHECK(attributes(frame).x == initial.x &&
		attributes(frame).y == initial.y);
	shortcut(XK_a, ControlMask);
	shortcut(XK_BackSpace, 0);
	shortcut(XK_Return, 0);
	CHECK(named_popup("owm resize/move"));
	type_command("oops");
	shortcut(XK_Return, 0);
	CHECK(named_popup("owm resize/move"));
	shortcut(XK_a, ControlMask);
	type_command("-12");
	click(helper.x + helper.width / 2 + 40,
		helper.y + 6 + (helper.height - 12) / 4 + 8);
	type_command("42");
	shortcut(XK_Tab, 0);
	WAIT(attributes(frame).x == -12 && attributes(frame).y == 42);
	screenshot(image_name);
	shortcut(XK_Escape, 0);
	WAIT(!named_popup("owm resize/move"));
	CHECK(attributes(frame).x == initial.x &&
		attributes(frame).y == initial.y);

	start_keyboard_move();
	shortcut(XK_Tab, 0);
	type_command("32700");
	shortcut(XK_Tab, 0);
	type_command("-32700");
	shortcut(XK_Return, 0);
	WAIT(!named_popup("owm resize/move"));
	CHECK(attributes(frame).x == 1280 - 48 && attributes(frame).y == 0);
	XMoveWindow(dpy, app, initial.x, initial.y);
	XFlush(dpy);
	WAIT(attributes(frame).x == initial.x &&
		attributes(frame).y == initial.y);

	CHECK(attributes(app).width == content.width &&
		attributes(app).height == content.height);
}

static void
check_mouse_move(Window app, int step, int resize_step)
{
	Window frame;
	XWindowAttributes initial, content;
	int growth;

	frame = parent_of(app);
	initial = attributes(frame);
	content = attributes(app);
	/*
	 * Mouse entry uses the same titlebar menu and releases both
	 * grabs.
	 */
	right_click(initial.x + 90, initial.y + FRAME_BORDER + 8);
	WAIT(named_popup("owm window menu"));
	screenshot(step == 13 ? "custom-window-menu" : "window-menu");
	shortcut(XK_Home, 0);
	shortcut(XK_Return, 0);
	WAIT(named_popup("owm resize/move"));
	shortcut(XK_Left, ShiftMask);
	shortcut(XK_Up, 0);
	shortcut(XK_Right, ControlMask);

	growth = (resize_step + 7) / 8 * 8;

	WAIT(attributes(app).width == content.width + growth);
	shortcut(XK_Return, 0);
	WAIT(!named_popup("owm resize/move"));
	CHECK(attributes(frame).x == initial.x - 1 &&
		attributes(frame).y == initial.y - step);
	CHECK(XGrabKeyboard(dpy, root, False, GrabModeAsync, GrabModeAsync,
		CurrentTime) == GrabSuccess);
	XUngrabKeyboard(dpy, CurrentTime);
	CHECK(XGrabPointer(dpy, root, False, ButtonPressMask, GrabModeAsync,
		GrabModeAsync, None, None, CurrentTime) == GrabSuccess);
	XUngrabPointer(dpy, CurrentTime);
	XFlush(dpy);
}

static void
check_move_maximized(Window app)
{
	Window frame;
	XWindowAttributes saved;

	frame = parent_of(app);
	/*
	 * Cancellation restores maximization and its saved normal
	 * geometry.
	 */
	saved = attributes(frame);

	shortcut(XK_m, Mod4Mask | ShiftMask);
	WAIT(state(app, "_NET_WM_STATE_MAXIMIZED_VERT"));
	start_keyboard_move();
	shortcut(XK_Right, 0);
	WAIT(!state(app, "_NET_WM_STATE_MAXIMIZED_VERT"));
	shortcut(XK_Escape, 0);
	WAIT(state(app, "_NET_WM_STATE_MAXIMIZED_VERT"));
	start_keyboard_move();
	shortcut(XK_Tab, 0);
	type_command("321");
	shortcut(XK_Tab, 0);
	type_command("123");
	shortcut(XK_Tab, 0);
	WAIT(!state(app, "_NET_WM_STATE_MAXIMIZED_VERT"));
	CHECK(attributes(frame).x == 321 && attributes(frame).y == 123);
	shortcut(XK_Escape, 0);
	WAIT(state(app, "_NET_WM_STATE_MAXIMIZED_VERT"));
	shortcut(XK_m, Mod4Mask | ShiftMask);
	WAIT(!state(app, "_NET_WM_STATE_MAXIMIZED_VERT"));
	CHECK(attributes(frame).x == saved.x &&
		attributes(frame).width == saved.width);
}

static void
check_fixed_size(Window app)
{
	XSizeHints hints = {0};

	/*
	 * Applications with a fixed size can move, but must not change
	 * size.
	 */
	hints.flags = PMinSize | PMaxSize;
	hints.min_width = 200;
	hints.max_width = 200;
	hints.min_height = 120;
	hints.max_height = 120;

	XSetWMNormalHints(dpy, app, &hints);
	XResizeWindow(dpy, app, 200, 120);
	XFlush(dpy);
	WAIT(attributes(app).width == 200 && attributes(app).height == 120);
	shortcut(XK_r, Mod4Mask);
	WAIT(named_popup("owm resize/move"));
	shortcut(XK_Right, ControlMask);
	shortcut(XK_Up, ControlMask | ShiftMask);
	CHECK(attributes(app).width == 200 && attributes(app).height == 120);
	shortcut(XK_Return, 0);
	WAIT(!named_popup("owm resize/move"));
}

static void
check_close_during_move(Window app)
{

	/*
	 * Closing an app during either modal interaction releases the
	 * grabs.
	 */
	shortcut(XK_w, Mod4Mask);
	WAIT(named_popup("owm window menu"));
	XDestroyWindow(dpy, app);
	XFlush(dpy);
	WAIT(count_clients() == 0 && !named_popup("owm window menu"));
	app = make_window("disappearing", "Close while moving", 0);
	XMapWindow(dpy, app);
	XFlush(dpy);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == app);
	start_keyboard_move();
	XDestroyWindow(dpy, app);
	XFlush(dpy);
	WAIT(count_clients() == 0 && !named_popup("owm resize/move"));
	shortcut(XK_p, Mod4Mask);
	WAIT(find_instance("owm-run"));
	shortcut(XK_Escape, 0);
	WAIT(count_clients() == 0);
}

void
test_keyboard_move(void)
{
	Window app;
	int setting, step, resize_step;

	for (setting = 0; setting < 2; setting++) {
		step = setting ? 13 : 10;
		resize_step = setting ? 1 : 10;
		write_config(
			"restore_session = false\n"
			"move_step = %d\nresize_step = %d\n"
			"resize_step = 0\nmove_step = 101\n"
			"focus_follows_mouse = perhaps\nunknown_option = true\n"
			"bind = \"Super+r window_move_resize\"\n",
			step, resize_step);
		start_wm();
		shortcut(XK_w, Mod4Mask);
		CHECK(!named_popup("owm window menu"));

		app = make_window("keyboard-move", "Precise placement", 0);
		XMapWindow(dpy, app);
		XFlush(dpy);
		WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == app);
		check_move_steps(app, step);
		check_move_fullscreen(app, step);
		check_coordinate_entry(app,
			setting ? "custom-coordinate-input"
				: "coordinate-input");
		check_mouse_move(app, step, resize_step);
		check_move_maximized(app);
		check_fixed_size(app);
		check_close_during_move(app);
		stop_wm();
	}

	CHECK(log_contains("resize_step: expected 1–100 pixels"));
	CHECK(log_contains("move_step: expected 1–100 pixels"));
	CHECK(log_contains("focus_follows_mouse: expected true or false"));
	CHECK(log_contains("unknown_option: unknown option"));
}
