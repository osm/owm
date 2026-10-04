#include "cases.h"
#include "harness.h"

static int	taskbar_button_pressed(Window window);

void
test_transient_stacking(void)
{
	Window app, dialog, child;

	start_wm();
	window_move_pointer(1200, 700);
	app = make_window("stack-app", "Application", 0);
	dialog = make_window("stack-dialog", "Dialog", 0);
	child = make_window("stack-child", "Nested dialog", 0);
	XSetTransientForHint(dpy, dialog, app);
	XSetTransientForHint(dpy, child, dialog);
	XMapWindow(dpy, app);
	XFlush(dpy);
	WAIT(count_clients() == 1);
	/* Client list order must not determine a dialog family's stacking. */
	XMapWindow(dpy, child);
	XFlush(dpy);
	WAIT(count_clients() == 2);
	XMapWindow(dpy, dialog);
	XFlush(dpy);
	WAIT(count_clients() == 3);
	message(app, "_NET_ACTIVE_WINDOW", 1, CurrentTime, 0);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == app);
	WAIT(stack_position(parent_of(child)) >
		stack_position(parent_of(dialog)));
	CHECK(stack_position(parent_of(dialog)) >
		stack_position(parent_of(app)));

	message(app, "_NET_WM_STATE", 1, atom("_NET_WM_STATE_FULLSCREEN"), 0);
	WAIT(state(app, "_NET_WM_STATE_FULLSCREEN"));
	WAIT(stack_position(parent_of(child)) >
		stack_position(parent_of(dialog)));
	CHECK(stack_position(parent_of(dialog)) >
		stack_position(parent_of(app)));
	message(dialog, "_NET_ACTIVE_WINDOW", 1, CurrentTime, 0);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == dialog);
	WAIT(stack_position(parent_of(child)) >
		stack_position(parent_of(dialog)));

	/* Malformed transient cycles must not trap the WM in recursion. */
	XSetTransientForHint(dpy, app, child);
	message(app, "_NET_ACTIVE_WINDOW", 1, CurrentTime, 0);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == app);
	stop_wm();
	XDestroyWindow(dpy, child);
	XDestroyWindow(dpy, dialog);
	XDestroyWindow(dpy, app);
	XSync(dpy, False);
}

void
test_focus_cycle(void)
{
	Window a, b, c, selected, popup, extra[9];
	XWindowAttributes box, screen;
	int i;
	KeyCode alt, shift;

	write_config("restore_session = false\nfocus_follows_mouse = true\n"
		"bind = \"Super+t window_focus_next\"\n"
		"bind = \"Super+Shift+t window_focus_prev\"\n");
	start_wm();
	window_move_pointer(1100, 600);

	a = make_window("cycle-a", "Cycle A", 0);
	b = make_window("cycle-b", "Cycle B", 0);
	c = make_window("cycle-c", "Cycle C", 0);
	XMapWindow(dpy, a);
	XMapWindow(dpy, b);
	XMapWindow(dpy, c);
	XFlush(dpy);
	WAIT(count_clients() == 3);
	message(a, "_NET_ACTIVE_WINDOW", 1, CurrentTime, 0);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == a);
	message(b, "_NET_ACTIVE_WINDOW", 1, CurrentTime, 0);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == b);
	message(c, "_NET_ACTIVE_WINDOW", 1, CurrentTime, 0);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == c);

	/* Separate presses toggle the two most recent windows. */
	shortcut(XK_Tab, Mod1Mask);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == b);
	shortcut(XK_Tab, Mod1Mask);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == c);

	alt = XKeysymToKeycode(dpy, XK_Alt_L);
	shift = XKeysymToKeycode(dpy, XK_Shift_L);
	fake_key(dpy, alt, True, 0);
	shortcut(XK_Tab, 0);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == b);
	WAIT((popup = named_popup("owm window switcher")) != None);
	box = attributes(popup);
	screen = attributes(root);
	CHECK(box.override_redirect);
	CHECK(box.x == (screen.width - box.width) / 2);
	CHECK(box.y == (screen.height - box.height) / 2);
	screenshot("window-switcher");
	shortcut(XK_Tab, 0);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == a);
	fake_key(dpy, shift, True, 0);
	shortcut(XK_Tab, 0);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == b);
	fake_key(dpy, shift, False, 0);
	shortcut(XK_Tab, 0);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == a);
	fake_key(dpy, alt, False, 0);
	XSync(dpy, False);
	WAIT(named_popup("owm window switcher") == None);
	pause_ms(100);
	shortcut(XK_Tab, Mod1Mask);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == c);
	shortcut(XK_Tab, Mod1Mask);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == a);

	/* Escape cancels, including when the selected window is fullscreen. */
	fake_key(dpy, alt, True, 0);
	shortcut(XK_Tab, 0);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == c);
	message(c, "_NET_WM_STATE", 1, atom("_NET_WM_STATE_FULLSCREEN"), 0);
	WAIT(state(c, "_NET_WM_STATE_FULLSCREEN"));
	WAIT((popup = named_popup("owm window switcher")) != None);
	CHECK(stack_position(popup) > stack_position(parent_of(c)));
	shortcut(XK_Escape, 0);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == a);
	WAIT(named_popup("owm window switcher") == None);
	message(c, "_NET_WM_STATE", 0, atom("_NET_WM_STATE_FULLSCREEN"), 0);
	WAIT(!state(c, "_NET_WM_STATE_FULLSCREEN"));
	fake_key(dpy, alt, False, 0);
	XSync(dpy, False);
	shortcut(XK_t, Mod4Mask);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == c);
	shortcut(XK_t, Mod4Mask);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == a);

	/* Removing a selected client must not leave a stale history entry. */
	fake_key(dpy, alt, True, 0);
	shortcut(XK_Tab, 0);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == c);
	XDestroyWindow(dpy, c);
	XFlush(dpy);
	WAIT(count_clients() == 2);
	shortcut(XK_Tab, 0);
	selected = value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW);
	CHECK(selected == a || selected == b);
	fake_key(dpy, alt, False, 0);
	XSync(dpy, False);
	WAIT(named_popup("owm window switcher") == None);
	pause_ms(100);
	shortcut(XK_Tab, Mod1Mask);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) ==
		(selected == a ? b : a));

	/* A long list stays on screen and tolerates all its windows closing. */
	for (i = 0; i < 9; i++) {
		extra[i] = make_window("cycle-extra", "Another window", 0);
		XMapWindow(dpy, extra[i]);
	}
	XFlush(dpy);
	WAIT(count_clients() == 11);
	fake_key(dpy, alt, True, 0);
	for (i = 0; i < 12; i++) {
		shortcut(XK_Tab, 0);
		WAIT((popup = named_popup("owm window switcher")) != None);
		box = attributes(popup);
		CHECK(box.y >= 0 && box.y + box.height <= screen.height);
	}
	for (i = 0; i < 9; i++)
		XDestroyWindow(dpy, extra[i]);
	XDestroyWindow(dpy, a);
	XDestroyWindow(dpy, b);
	XFlush(dpy);
	WAIT(count_clients() == 0);
	WAIT(named_popup("owm window switcher") == None);
	fake_key(dpy, alt, False, 0);
	XSync(dpy, False);
	stop_wm();
	CHECK(unlink(config_path) == 0);
}

void
test_pointer_focus(void)
{
	int enabled;

	for (enabled = 0; enabled < 2; enabled++) {
		Window a, b;

		write_config(
			"restore_session = false\n"
			"focus_follows_mouse = %s\n"
			"taskbar_hide_empty_desktops = false\ndesktops = 4\n",
			enabled ? "true" : "false");
		window_move_pointer(1200, 700);
		start_wm();

		a = make_window("hover-a", "Hover A", 0);
		b = make_window("hover-b", "Hover B", 0);

		XMapWindow(dpy, a);
		XMapWindow(dpy, b);
		XFlush(dpy);
		WAIT(count_clients() == 2);
		XMoveResizeWindow(dpy, a, 60, 60, 400, 280);
		XMoveResizeWindow(dpy, b, 250, 120, 400, 280);
		XFlush(dpy);
		WAIT(attributes(parent_of(a)).x == 60 &&
			attributes(parent_of(b)).x == 250);
		click(500, 250);
		WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == b);
		WAIT(taskbar_button_pressed(b) && !taskbar_button_pressed(a));
		window_move_pointer(100, 160);

		if (enabled) {
			Window focused;
			int revert;

			WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == a);

			XGetInputFocus(dpy, &focused, &revert);
			CHECK(focused == a);
			WAIT(taskbar_button_pressed(a) &&
				!taskbar_button_pressed(b));
			CHECK(stack_position(parent_of(a)) <
				stack_position(parent_of(b)));
			window_move_pointer(1200, 700);
			CHECK(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) ==
				a);
			window_move_pointer(1100, 786);
			CHECK(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) ==
				a);
			window_move_pointer(
				300, 130); /* Titlebar entry also focuses. */
			WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == b);
			WAIT(taskbar_button_pressed(b) &&
				!taskbar_button_pressed(a));
			window_move_pointer(100, 160);
			WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == a);
			click(100, 160);
			CHECK(stack_position(parent_of(a)) >
				stack_position(parent_of(b)));

			/*
			 * A held mouse button must not transfer focus across
			 * applications.
			 */
			fake_button(dpy, Button1, True, 0);
			window_move_pointer(600, 250);
			CHECK(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) ==
				a);
			fake_button(dpy, Button1, False, 0);
			XFlush(dpy);
		} else {
			CHECK(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) ==
				b);
		}

		/*
		 * Application requested focus uses the same bookkeeping as WM
		 * requests.
		 */
		window_move_pointer(1200, 700);
		XSetInputFocus(dpy, a, RevertToPointerRoot, CurrentTime);
		XFlush(dpy);
		WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == a);
		WAIT(taskbar_button_pressed(a) && !taskbar_button_pressed(b));
		XSetInputFocus(dpy, b, RevertToPointerRoot, CurrentTime);
		XFlush(dpy);
		WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == b);
		WAIT(taskbar_button_pressed(b) && !taskbar_button_pressed(a));
		shortcut(XK_Tab, Mod1Mask);
		WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == a);
		shortcut(XK_Tab, Mod1Mask);
		WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == b);

		XDestroyWindow(dpy, a);
		XDestroyWindow(dpy, b);
		XFlush(dpy);
		WAIT(count_clients() == 0);
		stop_wm();
	}
}

static int
taskbar_button_pressed(Window window)
{
	XImage *image = NULL;
	XColor color = {0};
	unsigned long *clients = NULL;
	unsigned long count;
	unsigned long index = 0;

	clients = property(root, "_NET_CLIENT_LIST", XA_WINDOW, &count);

	while (index < count && clients[index] != window)
		index++;
	CHECK(index < count && count <= 3);
	XFree(clients);

	image = XGetImage(dpy, taskbar_window(), 107 + index * 180, 4, 1, 1,
		AllPlanes, ZPixmap);

	CHECK(image);

	color.pixel = XGetPixel(image, 0, 0);

	XQueryColor(dpy, DefaultColormap(dpy, DefaultScreen(dpy)), &color);
	XDestroyImage(image);

	return color.red / 257 == 101 && color.green / 257 == 101 &&
		color.blue / 257 == 94;
}
