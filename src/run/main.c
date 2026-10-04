/*
 * Small X11 application launcher. Commands use shell quoting rules, not a
 * shell.
 */
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <err.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "builtin-icons.h"
#include "process.h"
#include "string-util.h"
#include "text-input.h"
#include "ui-config.h"
#include "ui.h"
#include "x11-input.h"

static struct {
	int		width;
	int		height;
	int		button_y;
	int		button_width;
	int		button_height;
	struct ui_rect	input;
} layout;

static struct ui ui;
static struct text_input entry;
static Window window;
static struct x11_input input;
static Atom protocols_atom;
static char error[256];
static int done, focus, help_open, line_height;
static const char *prompt = "Type the name of the program to run";

static int		text_width(const char *);
static void		layout_init(void);
static struct ui_rect	button_bounds(int);
static void		draw(void);
static void		insert(const char *, size_t);
static int		launch(void);
static void		handle_button(XButtonEvent *);
static void		handle_key(XKeyEvent *);
static void		handle_event(XEvent, Atom, Atom);
static int		claim_dialog(Window, int, Atom);
static void		configure_dialog(int, Atom, Atom);
static void		run_events(Atom, Atom);

static int
text_width(const char *text)
{
	return font_width(ui.font, text, (int)strlen(text));
}

static void
layout_init(void)
{
	line_height = ui.font->ascent + ui.font->descent;
	layout.input.x = text_width("Open:") + 24;

	if (layout.input.x < 54)
		layout.input.x = 54;
	layout.input.y = 18 + 2 * (line_height + 2) + 12;
	layout.input.width = 48 * text_width("M");
	layout.button_width = text_width("Cancel") + 18;

	if (layout.button_width < 75)
		layout.button_width = 75;

	if (layout.input.width < 2 * layout.button_width + 6)
		layout.input.width = 2 * layout.button_width + 6;
	layout.input.height = line_height + 8;
	layout.button_height = line_height + 10;
	layout.button_y =
		layout.input.y + layout.input.height + line_height + 12;
	layout.width = layout.input.x + layout.input.width + 12;
	layout.height = layout.button_y + layout.button_height + 11;
}

static struct ui_rect
button_bounds(int index)
{
	struct ui_rect bounds;

	bounds.x = layout.width - 12 - 2 * layout.button_width - 6 +
		(index - 1) * (layout.button_width + 6);
	bounds.y = layout.button_y;
	bounds.width = layout.button_width;
	bounds.height = layout.button_height;

	return bounds;
}

static void
draw(void)
{
	struct canvas canvas = {0};
	int i;

	canvas.ui = &ui;
	canvas.window = window;

	ui_rect(canvas, 0, 0, layout.width, layout.height, ui.colors[FACE]);
	builtin_icon_draw(ui.display, canvas.window, ui.gc, 13, 15, 32,
		BUILTIN_TERMINAL, ui.colors);
	ui_label(canvas, layout.input.x, 18 + ui.font->ascent, prompt,
		ui.colors[BLACK]);
	ui_label(canvas, 10, layout.input.y + 4 + ui.font->ascent,
		"Open:", ui.colors[BLACK]);
	ui_rect(canvas, 10, layout.input.y + 6 + ui.font->ascent,
		font_width(ui.font, "O", 1), 1, ui.colors[BLACK]);
	ui_rect(canvas, layout.input.x, layout.input.y, layout.input.width,
		layout.input.height, ui.colors[WHITE]);
	ui_bevel(canvas, layout.input.x, layout.input.y, layout.input.width,
		layout.input.height, 1);
	text_input_draw(canvas, layout.input, &entry, (focus == 0));
	ui_label(canvas, 10,
		layout.input.y + layout.input.height + 4 + ui.font->ascent,
		error, ui.colors[BLACK]);

	if (*entry.text != '\0') {
		struct ui_rect bounds;

		XSetForeground(ui.display, ui.gc, ui.colors[BLACK]);

		bounds = button_bounds(1);

		XDrawRectangle(ui.display, window, ui.gc, bounds.x - 1,
			bounds.y - 1, bounds.width + 1, bounds.height + 1);
	}

	for (i = 1; i <= 2; i++)
		ui_button(canvas, button_bounds(i), i == 1 ? "OK" : "Cancel",
			focus == i, i != 1 || *entry.text != '\0');

	if (help_open) {
		ui_rect(canvas, 8, 8, layout.width - 16, layout.button_y - 14,
			ui.colors[FACE]);
		ui_bevel(canvas, 8, 8, layout.width - 16, layout.button_y - 14,
			0);
		ui_label(canvas, 16,
			14 + ui.font->ascent + 0 * (line_height + 4), prompt,
			ui.colors[BLACK]);
		ui_label(canvas, 16,
			14 + ui.font->ascent + 1 * (line_height + 4),
			"Press Enter or click OK to run the program.",
			ui.colors[BLACK]);
		ui_label(canvas, 16,
			14 + ui.font->ascent + 2 * (line_height + 4),
			"Use quotes around arguments containing spaces.",
			ui.colors[BLACK]);
		ui_label(canvas, 16,
			14 + ui.font->ascent + 3 * (line_height + 4),
			"Ctrl+V pastes. Escape or F1 closes this help.",
			ui.colors[BLACK]);
	}

	XFlush(ui.display);
}

static void
insert(const char *text, size_t length)
{
	if (text_input_insert(&entry, text, length))
		error[0] = '\0';
	else
		string_copy(error, "Command is too long.", sizeof(error));
}

static int
launch(void)
{
	return process_launch(
		ConnectionNumber(ui.display), entry.text, error, sizeof(error));
}

static void
handle_button(XButtonEvent *e)
{
	int i, x, y;

	x = e->x;
	y = e->y;

	help_open = 0;
	if (ui_contains(layout.input, x, y)) {
		focus = 0;
		text_input_click(&entry, ui.font, layout.input, x);
		if (e->button == Button2)
			x11_input_request_paste(&input, 1, e->time);
	}

	if (y < layout.button_y ||
		y >= layout.button_y + layout.button_height ||
		e->button != Button1)
		return;

	for (i = 1; i <= 2; i++) {
		if (!ui_contains(button_bounds(i), x, y))
			continue;

		focus = i;
		if (i == 1 && *entry.text != '\0')
			done = launch();
		if (i == 2)
			done = 1;
	}
}

static void
handle_key(XKeyEvent *e)
{
	KeySym key = NoSymbol;
	int ctrl, n;
	char text[TEXT_INPUT_SIZE];

	n = x11_input_key(&input, e, text, sizeof(text), &key);
	if (n < 0) {
		string_copy(error, "Command is too long.", sizeof(error));
		return;
	}
	ctrl = e->state & ControlMask;

	if (help_open) {
		if (key == XK_Escape || key == XK_F1)
			help_open = 0;
		return;
	}

	if (key == XK_F1) {
		help_open = 1;
		return;
	}

	if ((e->state & Mod1Mask) && (key == XK_o || key == XK_O)) {
		focus = 0;
		entry.selected = 1;
		return;
	}

	if (key == XK_Escape) {
		done = 1;
		return;
	}

	if (key == XK_Tab || key == XK_ISO_Left_Tab) {
		focus = (focus + ((e->state & ShiftMask) ? 2 : 1)) % 3;
		return;
	}

	if (key == XK_Return || key == XK_KP_Enter ||
		(key == XK_space && focus)) {
		if (focus == 2) {
			done = 1;
			return;
		}

		if (*entry.text != '\0')
			done = launch();

		return;
	}

	if (focus)
		return;

	if (ctrl && (key == XK_v || key == XK_V)) {
		x11_input_request_paste(&input, 0, e->time);
		return;
	}

	if (!text_input_key(&entry, key, e->state, text, n)) {
		string_copy(error, "Command is too long.", sizeof(error));
		return;
	}

	error[0] = '\0';
}

static void
handle_event(XEvent e, Atom delete, Atom help)
{
	if (e.type == ClientMessage && e.xclient.window == window &&
		e.xclient.message_type == protocols_atom &&
		e.xclient.format == 32) {
		if ((Atom)e.xclient.data.l[0] == delete) {
			done = 1;
			return;
		}

		if ((Atom)e.xclient.data.l[0] == help)
			help_open = !help_open;

		return;
	}

	if (e.type == ButtonPress) {
		handle_button(&e.xbutton);
		return;
	}

	if (e.type == SelectionNotify) {
		char *text = NULL;

		text = x11_input_paste(&input, &e.xselection);
		if (text != NULL) {
			insert(text, strlen(text));
			free(text);
		}

		return;
	}

	if (e.type == KeyPress)
		handle_key(&e.xkey);
}

static int
claim_dialog(Window root, int screen, Atom singleton)
{
	Window existing;

	/*
	 * Serialize creation so repeated shortcuts focus the existing dialog.
	 */
	XGrabServer(ui.display);

	existing = XGetSelectionOwner(ui.display, singleton);

	if (existing) {
		XEvent e = {0};

		e.xclient.type = ClientMessage;
		e.xclient.window = existing;
		e.xclient.message_type =
			XInternAtom(ui.display, "_NET_ACTIVE_WINDOW", False);
		e.xclient.format = 32;
		e.xclient.data.l[0] = 1;
		XSendEvent(ui.display, root, False,
			SubstructureRedirectMask | SubstructureNotifyMask, &e);
		XUngrabServer(ui.display);

		return 0;
	}

	window = XCreateSimpleWindow(ui.display, root,
		(DisplayWidth(ui.display, screen) - layout.width) / 2,
		(DisplayHeight(ui.display, screen) - layout.height) / 2,
		layout.width, layout.height, 0, ui.colors[BLACK],
		ui.colors[FACE]);
	XSetSelectionOwner(ui.display, singleton, window, CurrentTime);
	XUngrabServer(ui.display);

	return 1;
}

static void
configure_dialog(int screen, Atom delete, Atom help)
{
	XSizeHints hints = {0};
	XClassHint klass = {"owm-run", "OwmRun"};
	Atom skip, type;
	Atom protocols[2] = {0};

	XStoreName(ui.display, window, "Run");

	XSetClassHint(ui.display, window, &klass);

	hints.flags = PMinSize | PMaxSize | PPosition;
	hints.min_width = hints.max_width = layout.width;
	hints.min_height = hints.max_height = layout.height;
	hints.x = (DisplayWidth(ui.display, screen) - layout.width) / 2;
	hints.y = (DisplayHeight(ui.display, screen) - layout.height) / 2;
	XSetWMNormalHints(ui.display, window, &hints);

	type = XInternAtom(ui.display, "_NET_WM_WINDOW_TYPE_DIALOG", False);

	XChangeProperty(ui.display, window,
		XInternAtom(ui.display, "_NET_WM_WINDOW_TYPE", False), XA_ATOM,
		32, PropModeReplace, (unsigned char *)&type, 1);

	skip = XInternAtom(ui.display, "_NET_WM_STATE_SKIP_TASKBAR", False);

	XChangeProperty(ui.display, window,
		XInternAtom(ui.display, "_NET_WM_STATE", False), XA_ATOM, 32,
		PropModeReplace, (unsigned char *)&skip, 1);

	protocols_atom = XInternAtom(ui.display, "WM_PROTOCOLS", False);

	protocols[0] = delete;
	protocols[1] = help;

	XSetWMProtocols(ui.display, window, protocols, 2);
	XSelectInput(ui.display, window,
		ExposureMask | KeyPressMask | ButtonPressMask |
			x11_input_events(&input));
	XMapRaised(ui.display, window);
}

static void
run_events(Atom delete, Atom help)
{
	while (!done) {
		XEvent e;

		XNextEvent(ui.display, &e);
		if (x11_input_filter(&input, &e))
			continue;
		handle_event(e, delete, help);
		draw();
	}
}

int
main(void)
{
	Atom delete, help, singleton;
	Window root;
	int screen;
	char font_name[FONT_NAME_SIZE];

	if (setlocale(LC_CTYPE, "") == NULL)
		warnx("cannot set character locale; using the C locale");
	ui.display = XOpenDisplay(NULL);

	if (ui.display == NULL) {
		warnx("cannot open display");
		return 1;
	}

	screen = DefaultScreen(ui.display);
	root = RootWindow(ui.display, screen);
	singleton = XInternAtom(ui.display, "_OWM_RUN_DIALOG", False);

	ui_config_font(font_name);
	if (!ui_init(&ui, ui.display, root, font_name)) {
		XCloseDisplay(ui.display);
		return 1;
	}

	layout_init();

	if (!claim_dialog(root, screen, singleton)) {
		ui_free(&ui);
		XCloseDisplay(ui.display);
		return 0;
	}

	delete = XInternAtom(ui.display, "WM_DELETE_WINDOW", False);
	help = XInternAtom(ui.display, "_OWM_CONTEXT_HELP", False);
	if (!x11_input_init(&input, ui.display, window)) {
		ui_free(&ui);
		XDestroyWindow(ui.display, window);
		XCloseDisplay(ui.display);
		return 1;
	}
	configure_dialog(screen, delete, help);
	run_events(delete, help);

	x11_input_free(&input);
	ui_free(&ui);
	XDestroyWindow(ui.display, window);
	XCloseDisplay(ui.display);

	return 0;
}
