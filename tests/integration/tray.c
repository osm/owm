#include "cases.h"
#include "harness.h"

void
test_tray_tooltip(void)
{
	XWindowAttributes bar, bounds;
	Window after, focus, tip;
	int revert;

	window_move_pointer(500, 400);
	start_wm();

	bar = attributes(taskbar_window());

	XGetInputFocus(dpy, &focus, &revert);
	window_move_pointer(bar.width - 20, bar.y + bar.height / 2);
	pause_ms(100);
	CHECK(!named_popup("owm taskbar tooltip"));
	WAIT(named_popup("owm taskbar tooltip"));

	tip = named_popup("owm taskbar tooltip");
	bounds = attributes(tip);

	CHECK(bounds.override_redirect && bounds.x >= 0 &&
		bounds.x + bounds.width <= bar.width &&
		bounds.y + bounds.height < bar.y);
	CHECK(value(tip, "_NET_WM_WINDOW_TYPE", XA_ATOM) ==
		atom("_NET_WM_WINDOW_TYPE_TOOLTIP"));
	XGetInputFocus(dpy, &after, &revert);
	CHECK(after == focus);
	screenshot("tray-tooltip");

	window_move_pointer(500, 400);
	WAIT(!named_popup("owm taskbar tooltip"));
	window_move_pointer(bar.width - 20, bar.y + bar.height / 2);
	WAIT(named_popup("owm taskbar tooltip"));
	fake_button(dpy, Button1, True, 0);
	fake_button(dpy, Button1, False, 0);
	XFlush(dpy);
	WAIT(!named_popup("owm taskbar tooltip"));
	pause_ms(700);
	CHECK(!named_popup("owm taskbar tooltip"));
	stop_wm();
}

void
test_taskbar_tooltip(void)
{
	XWindowAttributes bar, bounds;
	Window after, app, focus, tip;
	int i, revert;
	char title[2048];

	write_config("restore_session = false\ndesktops = 1\n"
		"tray_battery = false\ntray_wireguard = false\n"
		"tray_clock = false\n");
	window_move_pointer(500, 400);
	start_wm();

	title[0] = '\0';
	for (i = 0; i < 12; i++)
		string_copy(title + strlen(title),
			"A long browser title with UTF8 caf\303\251 text ",
			sizeof(title) - strlen(title));
	app = make_window("tooltip-title", title, 0);
	XChangeProperty(dpy, app, atom("_NET_WM_NAME"), atom("UTF8_STRING"), 8,
		PropModeReplace, (unsigned char *)title, (int)strlen(title));
	XMapWindow(dpy, app);
	XFlush(dpy);
	WAIT(count_clients() == 1);
	bar = attributes(taskbar_window());
	XGetInputFocus(dpy, &focus, &revert);
	window_move_pointer(30, bar.y + bar.height / 2);
	pause_ms(100);
	CHECK(!named_popup("owm taskbar tooltip"));
	WAIT(named_popup("owm taskbar tooltip"));
	tip = named_popup("owm taskbar tooltip");
	bounds = attributes(tip);
	CHECK(bounds.width > 180 && bounds.width <= bar.width);
	CHECK(bounds.height > 40 && bounds.x >= 0 && bounds.y >= 0);
	CHECK(bounds.x + bounds.width <= bar.width &&
		bounds.y + bounds.height < bar.y);
	XGetInputFocus(dpy, &after, &revert);
	CHECK(after == focus);
	screenshot("taskbar-title-tooltip");

	XChangeProperty(dpy, app, atom("_NET_WM_NAME"), atom("UTF8_STRING"), 8,
		PropModeReplace, (unsigned char *)"Short title", 11);
	XFlush(dpy);
	WAIT(named_popup("owm taskbar tooltip") &&
		attributes(tip).width < 180 && attributes(tip).height < 40);

	window_move_pointer(500, 400);
	WAIT(!named_popup("owm taskbar tooltip"));
	window_move_pointer(30, bar.y + bar.height / 2);
	WAIT(named_popup("owm taskbar tooltip"));
	fake_button(dpy, Button1, True, 0);
	fake_button(dpy, Button1, False, 0);
	XFlush(dpy);
	WAIT(!named_popup("owm taskbar tooltip"));
	pause_ms(700);
	CHECK(!named_popup("owm taskbar tooltip"));

	window_move_pointer(500, 400);
	window_move_pointer(30, bar.y + bar.height / 2);
	WAIT(named_popup("owm taskbar tooltip"));
	XDestroyWindow(dpy, app);
	XFlush(dpy);
	WAIT(count_clients() == 0);
	WAIT(!named_popup("owm taskbar tooltip"));
	stop_wm();
}

void
test_tray_options(void)
{
	XWindowAttributes bar;
	int clock;

	for (clock = 0; clock <= 1; clock++) {
		write_config(
			"restore_session = false\ntray_battery = false\n"
			"tray_wireguard = false\ntray_clock = %s\n"
			"tray_battery = perhaps\ntray_wireguard = perhaps\n"
			"tray_clock = perhaps\n",
			clock ? "true" : "false");
		window_move_pointer(500, 400);
		start_wm();
		bar = attributes(taskbar_window());
		window_move_pointer(bar.width - 20, bar.y + bar.height / 2);
		if (clock)
			WAIT(named_popup("owm taskbar tooltip"));
		else {
			pause_ms(800);
			CHECK(!named_popup("owm taskbar tooltip"));
		}
		stop_wm();
	}

	CHECK(log_contains("tray_battery: expected true or false"));
	CHECK(log_contains("tray_wireguard: expected true or false"));
	CHECK(log_contains("tray_clock: expected true or false"));
}
