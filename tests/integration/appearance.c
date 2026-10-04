#include "../../src/shared/font.h"
#include "cases.h"
#include "harness.h"

static void	check_font_targets(void);
static void	check_frame_bevel(Window);
static void	check_title_baselines(Window);
static void	check_scaled_task_icon(Window, int);

static void
check_frame_bevel(Window app)
{
	XImage *frame_pixels = NULL, *image = NULL;
	XWindowAttributes outer;
	Window app_frame;
	int title, side, seams[2];

	app_frame = parent_of(app);
	outer = attributes(app_frame);
	title = outer.height - attributes(app).height - 2 * FRAME_BORDER;

	pause_ms(100);

	/*
	 * Corner marks must continue the visible button bevel, not step
	 * sideways where the outer border meets the titlebar.
	 */
	frame_pixels = XGetImage(dpy, app_frame, 0, 0, outer.width,
		outer.height, AllPlanes, ZPixmap);

	CHECK(frame_pixels);

	seams[0] = FRAME_BORDER + title;
	seams[1] = outer.width - FRAME_BORDER - title;

	for (side = 0; side < 2; side++) {
		int offset;

		for (offset = -2; offset < 2; offset++) {
			unsigned long bevel;
			int x;
			int y;

			x = seams[side] + offset;
			bevel = XGetPixel(
				frame_pixels, x, FRAME_BORDER + title / 2);

			for (y = 1; y < FRAME_BORDER; y++) {
				CHECK(XGetPixel(frame_pixels, x, y) == bevel);
				CHECK(XGetPixel(frame_pixels, x,
					outer.height - FRAME_BORDER + y -
						1) == bevel);
			}
		}
	}

	XDestroyImage(frame_pixels);

	image = XGetImage(dpy, app_frame, FRAME_BORDER, FRAME_BORDER, 1, 1,
		AllPlanes, ZPixmap);

	CHECK(image && XGetPixel(image, 0, 0) == 0xd6d2c7);
	XDestroyImage(image);
	CHECK(title >= 20);
}

static void
check_title_baselines(Window app)
{
	XImage *image = NULL;
	XWindowAttributes outer;
	Window app_frame;
	int title, capital_top;
	unsigned int i;
	const char *titles[] = {"H", "Hgp", "dumpinen - Google Chrome", "st"};

	app_frame = parent_of(app);
	outer = attributes(app_frame);
	title = outer.height - attributes(app).height - 2 * FRAME_BORDER;

	/*
	 * Descenders must not lift the main letter body. Capitals stay
	 * centered and share their top edge with the same text plus
	 * tails.
	 */

	capital_top = -1;

	for (i = 0; i < sizeof(titles) / sizeof(*titles); i++) {
		unsigned long background;
		int bottom, left, right, top, y;

		XStoreName(dpy, app, titles[i]);
		XFlush(dpy);
		pause_ms(100);
		image = XGetImage(dpy, app_frame, 0, FRAME_BORDER, outer.width,
			title, AllPlanes, ZPixmap);
		CHECK(image);

		left = FRAME_BORDER + title + 4;
		right = outer.width - FRAME_BORDER - 2 * title - 4;
		top = title;
		bottom = -1;
		background = XGetPixel(image, left, title / 2);

		for (y = 2; y < title - 2; y++) {
			int x;

			for (x = left; x < right; x++) {
				if (XGetPixel(image, x, y) == background)
					continue;
				if (y < top)
					top = y;
				if (y > bottom)
					bottom = y;
			}
		}

		CHECK(bottom >= top);
		if (i == 0) {
			CHECK(abs(top - (title - bottom - 1)) <= 1);
			capital_top = top;
		} else if (i == 1) {
			CHECK(top == capital_top);
		}
		CHECK(top >= 2 && bottom < title - 2);
		XDestroyImage(image);
	}
}

static void
check_scaled_task_icon(Window app, int size)
{
	XImage *image = NULL;
	unsigned long icon[2 + 16 * 16] = {16, 16};
	Window bar;
	int bar_height, icon_size, icon_x, i;

	/*
	 * App icons must grow with the bar, including native 16px art.
	 */

	for (i = 2; i < 2 + 16 * 16; i++)
		icon[i] = 0xffff0000;
	XChangeProperty(dpy, app, atom("_NET_WM_ICON"), XA_CARDINAL, 32,
		PropModeReplace, (unsigned char *)icon, 2 + 16 * 16);
	XFlush(dpy);
	pause_ms(100);

	bar = taskbar_window();
	bar_height = attributes(bar).height;

	CHECK(bar_height >= (size == 10 ? 28 : 44));

	icon_size = 16 * bar_height / 28;
	icon_x = 8 + 4 * (24 * bar_height / 28) + 5 * bar_height / 28 + 1;

	image = XGetImage(dpy, bar, icon_x + icon_size - 2, bar_height / 2, 1,
		1, AllPlanes, ZPixmap);
	CHECK(image && XGetPixel(image, 0, 0) == 0xff0000);
	XDestroyImage(image);
}

/* A clip from one draw must not leak into a different drawing target. */
static void
check_font_targets(void)
{
	struct text_font *font = NULL;
	Pixmap first, second;
	GC gc;
	XRectangle clip = {0, 0, 1, 1};
	XImage *expected = NULL, *actual = NULL;
	int x, y, ink = 0;

	font = font_open(dpy, "Liberation Mono:size=10");
	CHECK(font != NULL);
	first = XCreatePixmap(
		dpy, root, 80, 40, DefaultDepth(dpy, DefaultScreen(dpy)));
	second = XCreatePixmap(
		dpy, root, 80, 40, DefaultDepth(dpy, DefaultScreen(dpy)));
	gc = XCreateGC(dpy, first, 0, NULL);
	XSetForeground(dpy, gc, BlackPixel(dpy, DefaultScreen(dpy)));
	XFillRectangle(dpy, first, gc, 0, 0, 80, 40);
	XFillRectangle(dpy, second, gc, 0, 0, 80, 40);
	font_draw(font, first, 5, 25, "Test", 4,
		WhitePixel(dpy, DefaultScreen(dpy)), NULL);
	expected = XGetImage(dpy, first, 0, 0, 80, 40, AllPlanes, ZPixmap);
	CHECK(expected != NULL);
	font_draw(font, first, 5, 25, "Test", 4,
		WhitePixel(dpy, DefaultScreen(dpy)), &clip);
	font_draw(font, second, 5, 25, "Test", 4,
		WhitePixel(dpy, DefaultScreen(dpy)), NULL);
	actual = XGetImage(dpy, second, 0, 0, 80, 40, AllPlanes, ZPixmap);
	CHECK(actual != NULL);
	for (y = 0; y < 40; y++)
		for (x = 0; x < 80; x++) {
			CHECK(XGetPixel(expected, x, y) ==
				XGetPixel(actual, x, y));
			ink |= XGetPixel(actual, x, y) !=
				BlackPixel(dpy, DefaultScreen(dpy));
		}
	CHECK(ink);
	XDestroyImage(expected);
	XDestroyImage(actual);
	font_forget_drawable(font, first);
	XFreePixmap(dpy, first);
	font_forget_drawable(font, second);
	XFreePixmap(dpy, second);
	font_close(font);
	XFreeGC(dpy, gc);
}

void
test_fonts(void)
{
	int previous_height, previous_menu, previous_width, size;

	check_font_targets();
	previous_width = 0;
	previous_height = 0;
	previous_menu = 0;

	for (size = 10; size <= 18; size += 8) {
		XWindowAttributes bounds, frame;
		Window app, dialog;
		int menu_width;

		write_config(
			"restore_session = false\n"
			"font = \"Liberation Mono:size=%d\"\n"
			"taskbar_height = %d\n"
			"taskbar_hide_empty_desktops = false\ndesktops = 4\n",
			size, size == 10 ? 28 : 44);
		start_wm();
		right_click(600, 300);
		WAIT(desktop_menu());

		menu_width = attributes(desktop_menu()).width;

		CHECK(menu_width > previous_menu);
		previous_menu = menu_width;
		shortcut(XK_Escape, 0);
		shortcut(XK_p, Mod4Mask);
		WAIT(find_instance("owm-run"));

		dialog = find_instance("owm-run");
		bounds = attributes(dialog);
		frame = attributes(parent_of(dialog));

		CHECK(bounds.width > previous_width &&
			bounds.height > previous_height);

		if (size == 18)
			CHECK(frame.height - bounds.height > 28);
		previous_width = bounds.width;
		previous_height = bounds.height;
		screenshot(size == 10 ? "font-run" : "font-large-run");
		shortcut(XK_F1, 0);
		screenshot(size == 10 ? "font-help" : "font-large-help");
		shortcut(XK_Escape, 0);
		shortcut(XK_Escape, 0);
		WAIT(count_clients() == 0);

		app = make_window("scaled-caption", "Scaled titlebar", 0);

		XResizeWindow(dpy, app, 640, 240);
		XMapWindow(dpy, app);
		XFlush(dpy);
		WAIT(parent_of(app) != root);

		check_frame_bevel(app);
		check_title_baselines(app);
		check_scaled_task_icon(app, size);

		screenshot(
			size == 10 ? "font-titlebar" : "font-large-titlebar");
		caption(app, CAPTION_MENU);
		WAIT(named_popup("owm window menu"));
		shortcut(XK_Escape, 0);

		caption(app, CAPTION_MAXIMIZE);
		WAIT(state(app, "_NET_WM_STATE_MAXIMIZED_VERT"));
		caption(app, CAPTION_MAXIMIZE);
		WAIT(!state(app, "_NET_WM_STATE_MAXIMIZED_VERT"));
		caption(app, CAPTION_MINIMIZE);
		WAIT(state(app, "_NET_WM_STATE_HIDDEN"));
		message(app, "_NET_ACTIVE_WINDOW", 2, CurrentTime, 0);
		WAIT(!state(app, "_NET_WM_STATE_HIDDEN"));
		close_from_menu(app);
		CHECK(received_protocol(app, "WM_DELETE_WINDOW"));
		XDestroyWindow(dpy, app);
		XFlush(dpy);
		WAIT(count_clients() == 0);
		stop_wm();
	}
}

void
test_window_appearance(void)
{
	int size;

	for (size = 10; size <= 18; size += 8) {
		XImage *background = NULL;
		XImage *pixel = NULL;
		Window app, run;

		write_config(
			"restore_session = false\n"
			"font = \"Liberation Mono:size=%d\"\n"
			"titlebar_height = 24\ntaskbar_height = 34\n"
			"desktop_icon = \"terminal | Terminal | true | "
			"terminal\"\n"
			"desktop_icon = \"appearance-app | Applications | true "
			"| \"\n"
			"%s"
			"background_color = \"#12gg00\"\n"
			"background_color = \"#12345678\"\n"
			"background_color = \"\"\n",
			size,
			size == 18 ? "background_color = \"#234567\"\n" : "");
		start_wm();

		background = XGetImage(
			dpy, root, 1200, 100, 1, 1, AllPlanes, ZPixmap);

		CHECK(background &&
			XGetPixel(background, 0, 0) ==
				(size == 18 ? 0x234567 : 0x3e515b));
		XDestroyImage(background);

		app = make_window("appearance-app", "Unix workstation", 0);

		XMapWindow(dpy, app);
		XFlush(dpy);
		WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == app);
		XMoveWindow(dpy, app, 180, 70);
		XFlush(dpy);
		WAIT(attributes(parent_of(app)).x == 180);
		caption(app, CAPTION_MAXIMIZE);
		WAIT(state(app, "_NET_WM_STATE_MAXIMIZED_VERT"));
		caption(app, CAPTION_MAXIMIZE);
		WAIT(!state(app, "_NET_WM_STATE_MAXIMIZED_VERT"));
		caption(app, CAPTION_MINIMIZE);
		WAIT(state(app, "_NET_WM_STATE_HIDDEN"));
		message(app, "_NET_ACTIVE_WINDOW", 1, CurrentTime, 0);
		WAIT(!state(app, "_NET_WM_STATE_HIDDEN"));
		shortcut(XK_p, Mod4Mask);
		WAIT(find_instance("owm-run"));

		run = find_instance("owm-run");

		pause_ms(100);

		pixel = XGetImage(dpy, run, 2, 2, 1, 1, AllPlanes, ZPixmap);

		CHECK(pixel && XGetPixel(pixel, 0, 0) == 0xaca99f);
		XDestroyImage(pixel);
		screenshot(
			size == 10 ? "appearance-run" : "appearance-large-run");
		right_click(1100, 100);
		WAIT(desktop_menu());
		screenshot(size == 10 ? "appearance-desktop"
				: "appearance-large-desktop");
		shortcut(XK_Escape, 0);
		caption(run, CAPTION_MENU);
		WAIT(named_popup("owm window menu"));
		CHECK(find_instance("owm-run") == run);
		shortcut(XK_End, 0);
		shortcut(XK_Return, 0);
		WAIT(!find_instance("owm-run"));
		caption(app, CAPTION_MENU);
		WAIT(named_popup("owm window menu"));
		CHECK(!received_protocol(app, "WM_DELETE_WINDOW"));
		screenshot(size == 10 ? "appearance-caption-menu"
				: "appearance-large-caption-menu");
		shortcut(XK_Home, 0);
		shortcut(XK_Return, 0);
		WAIT(named_popup("owm resize/move"));
		shortcut(XK_Escape, 0);
		WAIT(!named_popup("owm resize/move"));
		caption(app, CAPTION_MENU);
		WAIT(named_popup("owm window menu"));
		shortcut(XK_End, 0);
		shortcut(XK_Return, 0);
		CHECK(received_protocol(app, "WM_DELETE_WINDOW"));
		XDestroyWindow(dpy, app);
		XFlush(dpy);
		WAIT(count_clients() == 0);
		stop_wm();
	}
}
