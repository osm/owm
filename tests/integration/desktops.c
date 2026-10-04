#include "cases.h"
#include "harness.h"

static void	open_desktop_submenu(void);

void
test_desktops_and_keys(void)
{
	unsigned long *areas = NULL, n;
	unsigned long desktop;
	Window a, b, dialog;
	int pager;

	CHECK(access(session_path, F_OK) < 0 && errno == ENOENT);
	write_config("taskbar_hide_empty_desktops = false\ndesktops = 4\n");
	start_wm();
	CHECK(value(root, "_NET_NUMBER_OF_DESKTOPS", XA_CARDINAL) == 4);

	areas = property(root, "_NET_WORKAREA", XA_CARDINAL, &n);

	CHECK(n == 16 && areas[2] == 1280 && areas[3] == 772 &&
		areas[15] == 772);

	XFree(areas);

	a = make_window("desk-a", "Desktop one", 0);
	b = make_window("desk-b", "Desktop two", 0);
	desktop = 1;

	XChangeProperty(dpy, b, atom("_NET_WM_DESKTOP"), XA_CARDINAL, 32,
		PropModeReplace, (unsigned char *)&desktop, 1);
	XMapWindow(dpy, a);
	XMapWindow(dpy, b);
	XFlush(dpy);
	WAIT(count_clients() == 2);
	CHECK(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 0);
	CHECK(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == a);
	CHECK(attributes(parent_of(b)).map_state == IsUnmapped &&
		!state(b, "_NET_WM_STATE_HIDDEN"));

	shortcut(XK_2, Mod4Mask);
	WAIT(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 1);
	CHECK(attributes(parent_of(a)).map_state == IsUnmapped &&
		!state(a, "_NET_WM_STATE_HIDDEN"));
	CHECK(attributes(parent_of(b)).map_state == IsViewable);
	shortcut(XK_3, Mod4Mask | ShiftMask);
	WAIT(value(b, "_NET_WM_DESKTOP", XA_CARDINAL) == 2);
	CHECK(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 1);
	CHECK(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == None);
	shortcut(XK_Right, Mod4Mask);
	WAIT(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 2);
	CHECK(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == b);

	dialog = make_window("desktop-dialog", "Follows its parent", 0);

	XSetTransientForHint(dpy, dialog, b);
	XMapWindow(dpy, dialog);
	XFlush(dpy);
	WAIT(count_clients() == 3);
	shortcut(XK_4, Mod4Mask | ShiftMask);
	WAIT(value(b, "_NET_WM_DESKTOP", XA_CARDINAL) == 3 &&
		value(dialog, "_NET_WM_DESKTOP", XA_CARDINAL) == 3);
	CHECK(attributes(parent_of(dialog)).map_state == IsUnmapped);
	shortcut(XK_4, Mod4Mask);
	WAIT(attributes(parent_of(dialog)).map_state == IsViewable);
	caption(dialog, CAPTION_MINIMIZE);
	WAIT(state(b, "_NET_WM_STATE_HIDDEN"));
	shortcut(XK_1, Mod4Mask);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == a);
	shortcut(XK_4, Mod4Mask);
	CHECK(attributes(parent_of(b)).map_state == IsUnmapped &&
		state(b, "_NET_WM_STATE_HIDDEN"));
	click(170, 786);
	WAIT(attributes(parent_of(b)).map_state == IsViewable &&
		attributes(parent_of(dialog)).map_state == IsViewable);
	XDestroyWindow(dpy, dialog);
	XFlush(dpy);
	WAIT(count_clients() == 2);

	/* Clicking the pager is independent of keyboard shortcuts. */
	pager = 4;

	click(pager + 10, 786);
	WAIT(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 0);
	CHECK(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == a);
	fake_key(dpy, XKeysymToKeycode(dpy, XK_Shift_L), True, 0);
	click(pager + 24 + 10, 786);
	fake_key(dpy, XKeysymToKeycode(dpy, XK_Shift_L), False, 0);
	XFlush(dpy);
	WAIT(value(a, "_NET_WM_DESKTOP", XA_CARDINAL) == 1);
	CHECK(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 0);
	message(a, "_NET_WM_DESKTOP", 0, 0, 0);
	WAIT(attributes(parent_of(a)).map_state == IsViewable);
	click(pager + 3 * 24 + 10, 786);
	WAIT(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 3);
	message(a, "_NET_ACTIVE_WINDOW", 1, CurrentTime, 0);
	WAIT(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 0);
	message(b, "_NET_WM_DESKTOP", 0, 0, 0);
	WAIT(attributes(parent_of(b)).map_state == IsViewable);
	shortcut(XK_Tab, Mod1Mask);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == b);
	shortcut(XK_Tab, Mod1Mask | ShiftMask);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == a);
	shortcut(XK_Caps_Lock, 0);
	shortcut(XK_Num_Lock, 0);
	shortcut(XK_Right, Mod4Mask);
	WAIT(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 1);
	shortcut(XK_Caps_Lock, 0);
	shortcut(XK_Num_Lock, 0);
	message(b, "_NET_WM_DESKTOP", 3, 0, 0);
	message(a, "WM_CHANGE_STATE", IconicState, 0, 0);
	message(root, "_NET_CURRENT_DESKTOP", 3, CurrentTime, 0);
	WAIT(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 3 &&
		state(a, "_NET_WM_STATE_HIDDEN"));

	save_session();
	stop_wm();
	start_wm();
	WAIT(count_clients() == 2);
	CHECK(find_instance("desk-a") == a && find_instance("desk-b") == b);
	CHECK(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 3);
	CHECK(value(a, "_NET_WM_DESKTOP", XA_CARDINAL) == 0 &&
		value(b, "_NET_WM_DESKTOP", XA_CARDINAL) == 3);
	stop_wm();
	XDestroyWindow(dpy, a);
	XDestroyWindow(dpy, b);
	XSync(dpy, False);
	start_wm();
	WAIT(count_clients() == 2);
	a = find_instance("desk-a");
	b = find_instance("desk-b");
	CHECK(a && b);
	CHECK(value(a, "_NET_WM_DESKTOP", XA_CARDINAL) == 0 &&
		value(b, "_NET_WM_DESKTOP", XA_CARDINAL) == 3);
	CHECK(state(a, "_NET_WM_STATE_HIDDEN") &&
		attributes(parent_of(b)).map_state == IsViewable);
	message(a, "_NET_CLOSE_WINDOW", 0, 0, 0);
	message(b, "_NET_CLOSE_WINDOW", 0, 0, 0);
	WAIT(count_clients() == 0);
	stop_wm();
}

void
test_desktop_menu(void)
{
	XWindowAttributes frame, menu;
	Window run, survivor;

	CHECK(access(session_path, F_OK) < 0 && errno == ENOENT);
	start_wm();
	right_click(1100, 600);
	WAIT(desktop_menu());
	CHECK(count_clients() == 0);
	shortcut(XK_Escape, 0);
	WAIT(!desktop_menu());
	right_click(1100, 600);
	WAIT(desktop_menu());
	click(800, 500);
	WAIT(!desktop_menu());
	right_click(1279, 770);
	WAIT(desktop_menu());

	menu = attributes(desktop_menu());

	CHECK(menu.x >= 0 && menu.x + menu.width <= 1280 &&
		menu.y + menu.height <= 772);
	click(menu.x + menu.width / 2, menu.y + 3 + 12);
	CHECK(desktop_menu() && !find_instance("owm-run"));
	click(menu.x + 20, menu.y + 3 + 24 + 3);
	CHECK(desktop_menu() && !find_instance("owm-run"));
	shortcut(XK_Home, 0);
	screenshot("desktop-menu");
	shortcut(XK_Return, 0);
	WAIT(find_instance("owm-run") && !desktop_menu());

	run = find_instance("owm-run");
	frame = attributes(parent_of(run));

	right_click(frame.x + 40, frame.y + 50);
	CHECK(!desktop_menu());
	shortcut(XK_Escape, 0);
	WAIT(count_clients() == 0);
	right_click(1100, 600);
	WAIT(desktop_menu());
	menu = attributes(desktop_menu());
	click(menu.x + 20,
		menu.y + 3 + 24 + 7 + 24 + 7 +
			12); /* Save session, using the mouse. */
	WAIT(!desktop_menu());
	CHECK(access(session_path, F_OK) == 0);
	CHECK(unlink(session_path) == 0 && mkdir(session_path, 0700) == 0);
	right_click(1100, 600);
	WAIT(desktop_menu());
	shortcut(XK_Home, 0);
	shortcut(XK_Down, 0);
	shortcut(XK_Down, 0);
	shortcut(XK_Down, 0);
	shortcut(XK_Return, 0);
	CHECK(desktop_menu() &&
		value(root, "_NET_SUPPORTING_WM_CHECK", XA_WINDOW));
	shortcut(XK_Escape, 0);
	CHECK(rmdir(session_path) == 0);
	right_click(1100, 600);
	WAIT(desktop_menu());
	shortcut(XK_Home, 0);
	shortcut(XK_Down, 0);
	shortcut(XK_Down, 0);
	shortcut(XK_Down, 0);
	shortcut(XK_Return, 0);
	CHECK(wait_exit(wm_pid) == 0);
	wm_pid = 0;
	CHECK(access(session_path, F_OK) == 0);
	start_wm();
	CHECK(count_clients() == 0);

	survivor = make_window("menu-survivor", "Menu exit cleanup", 0);

	XMapWindow(dpy, survivor);
	XFlush(dpy);
	WAIT(count_clients() == 1);
	right_click(1100, 600);
	WAIT(desktop_menu());
	shortcut(XK_End, 0);
	shortcut(XK_Return, 0);
	CHECK(wait_exit(wm_pid) == 0);
	wm_pid = 0;
	CHECK(parent_of(survivor) == root &&
		attributes(survivor).map_state == IsViewable);
	XDestroyWindow(dpy, survivor);
	XSync(dpy, False);
	start_wm();
	CHECK(count_clients() ==
		0); /* Exit without saving preserved the empty snapshot. */
	stop_wm();
}

void
test_configurable_menu(void)
{
	FILE *file = NULL;
	char contents[32] = {0};
	char marker[PATH_MAX + 32];

	snprintf(marker, sizeof(marker), "%s/menu-command", output_path);

	write_config("restore_session = false\nclear_menu = true\n"
		"menu_separator = true\n"
		"menu_item = \"Terminal | exec xterm -name menu-custom\"\n"
		"menu_item = \"Broken | nonsense\"\n"
		"menu_item = \" | exec owm-run\"\n"
		"menu_item = \"Empty | exec \"\n"
		"menu_separator = true\n"
		"menu_item = \"Pipeline | exec printf menu-ok | cat > "
		"\\\"$OWM_TEST_OUTPUT/menu-command\\\"\"\n"
		"menu_item = \"Leave | session_exit\"\n"
		"menu_separator = true\n");
	start_wm();
	right_click(1100, 600);
	WAIT(desktop_menu());
	shortcut(XK_Home, 0);
	shortcut(XK_Return, 0);
	WAIT(find_instance("menu-custom") && !desktop_menu());
	message(find_instance("menu-custom"), "_NET_CLOSE_WINDOW", CurrentTime,
		0, 0);
	WAIT(count_clients() == 0);
	right_click(1100, 600);
	WAIT(desktop_menu());
	shortcut(XK_Home, 0);
	shortcut(XK_Down, 0);
	shortcut(XK_Return, 0);
	WAIT(access(marker, F_OK) == 0 && !desktop_menu());

	file = fopen(marker, "r");

	CHECK(file);

	CHECK(fread(contents, 1, sizeof(contents) - 1, file) == 7);
	fclose(file);
	CHECK(!strcmp(contents, "menu-ok"));
	right_click(1100, 600);
	WAIT(desktop_menu());
	shortcut(XK_End, 0);
	shortcut(XK_Return, 0);
	CHECK(wait_exit(wm_pid) == 0);
	wm_pid = 0;

	/* An explicitly empty menu disables the desktop popup. */
	write_config(
		"restore_session = false\nclear_menu = true\nmenu_separator = "
		"true\n");
	start_wm();
	right_click(1100, 600);
	CHECK(!desktop_menu());
	stop_wm();
}

void
test_hidden_desktops(void)
{
	Window app;
	unsigned long desktop = 4;

	write_config("restore_session = false\n");
	start_wm();
	CHECK(value(root, "_NET_NUMBER_OF_DESKTOPS", XA_CARDINAL) == 8);
	shortcut(XK_7, Mod4Mask);
	WAIT(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 6);
	shortcut(XK_8, Mod4Mask);
	WAIT(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 7);
	shortcut(XK_1, Mod4Mask);
	WAIT(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 0);
	app = make_window("hidden-desktops", "Desktop five", 0);
	XChangeProperty(dpy, app, atom("_NET_WM_DESKTOP"), XA_CARDINAL, 32,
		PropModeReplace, (unsigned char *)&desktop, 1);
	XMapWindow(dpy, app);
	XFlush(dpy);
	WAIT(count_clients() == 1);

	/* The visible buttons are one and five. */
	click(4 + 24 + 10, 786);
	WAIT(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 4);
	message(app, "WM_CHANGE_STATE", IconicState, 0, 0);
	WAIT(state(app, "_NET_WM_STATE_HIDDEN"));
	shortcut(XK_1, Mod4Mask);
	WAIT(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 0);
	click(4 + 24 + 10, 786);
	WAIT(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 4);

	/* A single desktop leaves the first slot for its window button. */
	click(4 + 10, 786);
	WAIT(!state(app, "_NET_WM_STATE_HIDDEN"));
	shortcut(XK_3, Mod4Mask);
	WAIT(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 2);
	click(4 + 10, 786);
	CHECK(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 2);
	click(4 + 24 + 10, 786);
	WAIT(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 4);

	/* Moving the window updates the displayed desktop numbers. */
	message(app, "_NET_WM_DESKTOP", 2, 0, 0);
	WAIT(value(app, "_NET_WM_DESKTOP", XA_CARDINAL) == 2);
	click(4 + 10, 786);
	WAIT(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 2);

	/* Sticky windows occupy every desktop. */
	message(app, "_NET_WM_DESKTOP", 0xffffffffUL, 0, 0);
	WAIT((value(app, "_NET_WM_DESKTOP", XA_CARDINAL) & 0xffffffffUL) ==
		0xffffffffUL);
	click(4 + 5 * 24 + 10, 786);
	WAIT(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 5);
	XDestroyWindow(dpy, app);
	XFlush(dpy);
	WAIT(count_clients() == 0);
	click(4 + 10, 786);
	CHECK(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 5);
	stop_wm();

	write_config("restore_session = false\ndesktops = 1\n"
		"taskbar_hide_single_desktop = false\n");
	start_wm();
	app = make_window("single-desktop", "Keep the switcher", 0);
	XMapWindow(dpy, app);
	XFlush(dpy);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == app);
	click(4 + 10, 786);
	CHECK(!state(app, "_NET_WM_STATE_HIDDEN"));
	click(4 + 24 + 4 + 10, 786);
	WAIT(state(app, "_NET_WM_STATE_HIDDEN"));
	XDestroyWindow(dpy, app);
	XFlush(dpy);
	WAIT(count_clients() == 0);
	stop_wm();
}

void
test_sticky_window(void)
{
	Window app, dialog;
	unsigned long desktop;
	int pass, i;

	start_wm();
	app = make_window("sticky", "Sticky window", 0);
	dialog = make_window("sticky-dialog", "Sticky dialog", 0);
	XSetTransientForHint(dpy, dialog, app);
	XMapWindow(dpy, app);
	XMapWindow(dpy, dialog);
	XFlush(dpy);
	WAIT(count_clients() == 2);

	for (pass = 0; pass < 2; pass++) {
		message(dialog, "_NET_ACTIVE_WINDOW", 1, CurrentTime, 0);
		WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == dialog);
		shortcut(XK_w, Mod4Mask);
		WAIT(named_popup("owm window menu"));
		shortcut(XK_Home, 0);
		for (i = 0; i < 3; i++)
			shortcut(XK_Down, 0);
		shortcut(XK_Return, 0);
		desktop = pass ? 1 : 0xffffffffUL;
		WAIT((value(app, "_NET_WM_DESKTOP", XA_CARDINAL) &
			0xffffffffUL) == desktop &&
			(value(dialog, "_NET_WM_DESKTOP", XA_CARDINAL) &
				0xffffffffUL) == desktop);
		WAIT(!named_popup("owm window menu"));
		message(root, "_NET_CURRENT_DESKTOP", pass ? 0 : 1, CurrentTime,
			0);
		WAIT(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) ==
			(unsigned long)(pass ? 0 : 1));
		CHECK(attributes(parent_of(app)).map_state ==
			(pass ? IsUnmapped : IsViewable));
		CHECK(attributes(parent_of(dialog)).map_state ==
			(pass ? IsUnmapped : IsViewable));
		if (pass == 0) {
			stop_wm();
			start_wm();
			WAIT(count_clients() == 2);
		}
	}
	XDestroyWindow(dpy, dialog);
	XDestroyWindow(dpy, app);
	XFlush(dpy);
	WAIT(count_clients() == 0);
	stop_wm();
}

void
test_desktop_submenu(void)
{
	int count;

	for (count = 1; count <= 8; count *= 2) {
		XWindowAttributes menu, sub;
		Window app;
		int row, side;

		write_config("restore_session = false\ndesktops = %d\n"
			"font = \"Liberation Mono:size=%d\"\n",
			count, count == 8 ? 30 : 10);
		start_wm();
		message(root, "_NET_CURRENT_DESKTOP", 0, CurrentTime, 0);
		WAIT(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 0);
		app = make_window("submenu", "Send this window", 0);

		XMapWindow(dpy, app);
		XFlush(dpy);
		WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == app);
		open_desktop_submenu();
		CHECK(count_clients() == 1);
		shortcut(XK_Left, 0);
		WAIT(!named_popup("owm desktop submenu"));
		CHECK(named_popup("owm window menu"));
		shortcut(XK_Return, 0);
		WAIT(named_popup("owm desktop submenu"));
		shortcut(XK_Escape, 0);
		WAIT(!named_popup("owm desktop submenu"));
		CHECK(named_popup("owm window menu"));
		shortcut(XK_Right, 0);
		WAIT(named_popup("owm desktop submenu"));
		shortcut(XK_End, 0);
		screenshot(count == 8 ? "desktop-submenu-large-font"
				: "desktop-submenu");
		shortcut(XK_Return, 0);
		WAIT(value(app, "_NET_WM_DESKTOP", XA_CARDINAL) ==
			(unsigned long)(count - 1));
		CHECK(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 0);
		CHECK(!named_popup("owm window menu") &&
			!named_popup("owm desktop submenu"));

		message(app, "_NET_WM_DESKTOP", 0, 0, 0);
		WAIT(value(app, "_NET_WM_DESKTOP", XA_CARDINAL) == 0);
		message(app, "_NET_ACTIVE_WINDOW", 1, CurrentTime, 0);
		WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == app);
		for (side = 0; side < 2; side++) {
			int edge, y;

			XMoveWindow(
				dpy, app, side ? 950 : 100, side ? 500 : 100);
			XFlush(dpy);
			WAIT(attributes(parent_of(app)).x ==
				(side ? 950 : 100));
			caption(app, CAPTION_MENU);
			WAIT(named_popup("owm window menu"));
			menu = attributes(named_popup("owm window menu"));
			row = (menu.height - 13) / 7;
			y = menu.y + 3 + 5 * row + row / 2;

			click(menu.x + menu.width / 2, y);
			WAIT(named_popup("owm desktop submenu"));
			sub = attributes(named_popup("owm desktop submenu"));
			CHECK(sub.x >= 0 && sub.x + sub.width <= 1280);
			CHECK(sub.y >= 0 &&
				sub.y + sub.height <=
					attributes(taskbar_window()).y);
			CHECK(side ? sub.x + sub.width <= menu.x
				: sub.x >= menu.x + menu.width);

			/*
			 * Cross both menus' padding instead of teleporting
			 * inside.
			 */
			edge = side ? menu.x + 2 : menu.x + menu.width - 3;
			window_move_pointer(edge, y);
			CHECK(named_popup("owm desktop submenu"));
			window_move_pointer(
				side ? sub.x + sub.width - 1 : sub.x, y);
			CHECK(named_popup("owm desktop submenu"));
			window_move_pointer(sub.x + sub.width / 2, y);
			click(sub.x + sub.width / 2,
				sub.y + 3 + (count > 1 ? row : 0) + row / 2);
			WAIT(!named_popup("owm window menu"));
			WAIT(value(app, "_NET_WM_DESKTOP", XA_CARDINAL) ==
				(unsigned long)(count > 1 ? 1 : 0));
			CHECK(value(root, "_NET_CURRENT_DESKTOP",
				XA_CARDINAL) == 0);
			message(app, "_NET_WM_DESKTOP", 0, 0, 0);
			WAIT(value(app, "_NET_WM_DESKTOP", XA_CARDINAL) == 0);
			message(app, "_NET_ACTIVE_WINDOW", 1, CurrentTime, 0);
			WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) ==
				app);
		}
		open_desktop_submenu();
		click(1, 1);
		WAIT(!named_popup("owm window menu") &&
			!named_popup("owm desktop submenu"));
		open_desktop_submenu();
		XDestroyWindow(dpy, app);
		XFlush(dpy);
		WAIT(count_clients() == 0 && !named_popup("owm window menu") &&
			!named_popup("owm desktop submenu"));
		stop_wm();
	}
}

static void
open_desktop_submenu(void)
{
	int i;

	shortcut(XK_w, Mod4Mask);
	WAIT(named_popup("owm window menu"));
	shortcut(XK_Home, 0);

	for (i = 0; i < 5; i++)
		shortcut(XK_Down, 0);

	shortcut(XK_Right, 0);
	WAIT(named_popup("owm desktop submenu"));
}

void
test_shortcuts(void)
{
	FILE *marker = NULL;
	XWindowAttributes frame;
	Window a;
	char contents[32] = {0};
	char path[PATH_MAX + 32];

	/* Start on desktop four, then shrink the configuration to two. */
	start_wm();
	message(root, "_NET_CURRENT_DESKTOP", 3, CurrentTime, 0);
	WAIT(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 3);
	stop_wm();

	write_config(
		"desktops = 2\nclear_bindings = true\ndouble_click_ms = 450\n"
		"bind = \"Ctrl+F8 desktop_switch 1\"\n"
		"bind = \"Ctrl+F8 desktop_switch 2\"\n"
		"bind = \"Ctrl+F8 invalid_action\"\n"
		"bind = \"Ctrl+F9 window_send_to_desktop 2\"\n"
		"bind = \"Ctrl+F10 window_maximize\"\n"
		"bind = \"Ctrl+F12 window_maximize\"\n"
		"bind = \"Ctrl+F12 none\"\n"
		"bind = \"Ctrl+F7 window_close\"\n"
		"bind = \"Ctrl+F11 exec printf shortcut-ok > "
		"\\\"$OWM_TEST_OUTPUT/key-command\\\"\"\n");
	start_wm();
	CHECK(value(root, "_NET_NUMBER_OF_DESKTOPS", XA_CARDINAL) == 2);
	CHECK(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) ==
		1); /* Shrunk from four desktops. */
	message(root, "_NET_CURRENT_DESKTOP", 0, CurrentTime, 0);
	WAIT(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 0);
	a = make_window("custom-key", "Custom shortcuts", 0);
	XMapWindow(dpy, a);
	XFlush(dpy);
	WAIT(count_clients() == 1);

	frame = attributes(parent_of(a));

	click(frame.x + 60, frame.y + 14);
	pause_ms(250);
	click(frame.x + 60, frame.y + 14);
	WAIT(state(a, "_NET_WM_STATE_MAXIMIZED_HORZ")); /* Uses configured 450
							ms, not the 300 ms
							default. */
	shortcut(XK_F10, ControlMask);
	WAIT(!state(a, "_NET_WM_STATE_MAXIMIZED_HORZ"));
	shortcut(XK_F12, ControlMask);
	CHECK(!state(a, "_NET_WM_STATE_MAXIMIZED_HORZ"));
	shortcut(XK_m, Mod4Mask | ShiftMask);
	CHECK(!state(a, "_NET_WM_STATE_MAXIMIZED_HORZ"));
	shortcut(XK_F10, ControlMask);
	WAIT(state(a, "_NET_WM_STATE_MAXIMIZED_HORZ"));
	shortcut(XK_F9, ControlMask);
	WAIT(value(a, "_NET_WM_DESKTOP", XA_CARDINAL) == 1);
	CHECK(attributes(parent_of(a)).map_state == IsUnmapped);
	shortcut(XK_F8, ControlMask);
	WAIT(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 1);
	shortcut(XK_F10, ControlMask);
	WAIT(!state(a, "_NET_WM_STATE_MAXIMIZED_HORZ"));
	shortcut(XK_F11, ControlMask);
	snprintf(path, sizeof(path), "%s/key-command", output_path);
	WAIT(access(path, R_OK) == 0);

	marker = fopen(path, "r");

	CHECK(marker);

	CHECK(fread(contents, 1, sizeof(contents) - 1, marker) ==
		strlen("shortcut-ok"));
	fclose(marker);
	CHECK(!strcmp(contents, "shortcut-ok"));
	shortcut(XK_F7, ControlMask);
	CHECK(received_protocol(a, "WM_DELETE_WINDOW"));
	XDestroyWindow(dpy, a);
	XFlush(dpy);
	WAIT(count_clients() == 0);
	stop_wm();
}
