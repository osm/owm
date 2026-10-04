#include "harness.h"

static int	class_matches(Window, const char *, const char *);
static Window	find_named_window(const char *, int);

Atom
atom(const char *name)
{
	return XInternAtom(dpy, name, False);
}

unsigned long *
property(Window window, const char *name, Atom type, unsigned long *n)
{
	Atom actual;
	unsigned long after;
	int format;
	unsigned char *data = NULL;

	*n = 0;

	XGetWindowProperty(dpy, window, atom(name), 0, 16384, False, type,
		&actual, &format, n, &after, &data);

	if (actual != type || format != 32) {
		if (data != NULL)
			XFree(data);
		*n = 0;
		return NULL;
	}

	return (unsigned long *)data;
}

unsigned long
value(Window window, const char *name, Atom type)
{
	unsigned long *data = NULL, n, result = 0;

	data = property(window, name, type, &n);

	if (data != NULL) {
		if (n)
			result = data[0];
		XFree(data);
	}

	return result;
}

int
state(Window window, const char *name)
{
	unsigned long *data = NULL, n;
	unsigned long i;
	int found = 0;

	data = property(window, "_NET_WM_STATE", XA_ATOM, &n);

	for (i = 0; data != NULL && i < n; i++)
		if (data[i] == atom(name))
			found = 1;

	if (data != NULL)
		XFree(data);

	return found;
}

int
count_clients(void)
{
	unsigned long *data = NULL, n;

	data = property(root, "_NET_CLIENT_LIST", XA_WINDOW, &n);

	if (data != NULL)
		XFree(data);

	return (int)n;
}

Window
parent_of(Window window)
{
	Window *children = NULL, parent, r;
	unsigned int n;

	CHECK(XQueryTree(dpy, window, &r, &parent, &children, &n));

	if (children != NULL)
		XFree(children);

	return parent;
}

XWindowAttributes
attributes(Window window)
{
	XWindowAttributes a;

	CHECK(XGetWindowAttributes(dpy, window, &a));

	return a;
}

void
message(Window window, const char *name, long a, long b, long c)
{
	XEvent e = {0};

	e.xclient.type = ClientMessage;
	e.xclient.window = window;
	e.xclient.message_type = atom(name);
	e.xclient.format = 32;
	e.xclient.data.l[0] = a;
	e.xclient.data.l[1] = b;
	e.xclient.data.l[2] = c;
	XSendEvent(dpy, root, False,
		SubstructureRedirectMask | SubstructureNotifyMask, &e);
	XFlush(dpy);
}

void
window_move_pointer(int x, int y)
{
	fake_motion(dpy, DefaultScreen(dpy), x, y, 0);
	XFlush(dpy);
	pause_ms(30);
}

void
click(int x, int y)
{
	window_move_pointer(x, y);
	fake_button(dpy, Button1, True, 0);
	XFlush(dpy);
	pause_ms(30);
	fake_button(dpy, Button1, False, 0);
	XFlush(dpy);
	pause_ms(60);
}

void
drag(int x, int y, int dx, int dy)
{
	window_move_pointer(x, y);
	fake_button(dpy, Button1, True, 0);
	XFlush(dpy);
	pause_ms(40);
	window_move_pointer(x + dx, y + dy);
	fake_button(dpy, Button1, False, 0);
	XFlush(dpy);
	pause_ms(60);
}

void
shortcut(KeySym key, unsigned int modifiers)
{
	const KeySym keys[] = {XK_Control_L, XK_Alt_L, XK_Shift_L, XK_Super_L};
	int i;
	const unsigned int masks[] = {
		ControlMask, Mod1Mask, ShiftMask, Mod4Mask};
	KeyCode code;

	for (i = 0; i < 4; i++)
		if (modifiers & masks[i])
			fake_key(dpy, XKeysymToKeycode(dpy, keys[i]), True, 0);

	code = XKeysymToKeycode(dpy, key);

	CHECK(code);
	fake_key(dpy, code, True, 0);
	fake_key(dpy, code, False, 0);

	for (i = 3; i >= 0; i--)
		if (modifiers & masks[i])
			fake_key(dpy, XKeysymToKeycode(dpy, keys[i]), False, 0);

	XFlush(dpy);
	pause_ms(100);
}

void
caption(Window window, enum caption_button button)
{
	XWindowAttributes frame;
	int title, x;

	frame = attributes(parent_of(window));
	title = frame.height - attributes(window).height - 2 * FRAME_BORDER;
	x = button == CAPTION_MENU ? FRAME_BORDER
				: frame.width - FRAME_BORDER -
			(button == CAPTION_MINIMIZE ? 2 : 1) * title;

	click(frame.x + x + title / 2, frame.y + FRAME_BORDER + title / 2);
}

void
close_from_menu(Window window)
{
	caption(window, CAPTION_MENU);
	shortcut(XK_End, 0);
	shortcut(XK_Return, 0);
}

static int
class_matches(Window window, const char *instance, const char *class_name)
{
	XClassHint hint = {0};
	int matches;

	matches = XGetClassHint(dpy, window, &hint) && hint.res_name != NULL &&
		strcmp(hint.res_name, instance) == 0 &&
		(class_name == NULL ||
			(hint.res_class != NULL &&
				strcmp(hint.res_class, class_name) == 0));
	if (hint.res_name != NULL)
		XFree(hint.res_name);
	if (hint.res_class != NULL)
		XFree(hint.res_class);

	return matches;
}

Window
find_instance(const char *name)
{
	unsigned long i;
	unsigned long n, *windows = NULL;
	Window result = None;

	windows = property(root, "_NET_CLIENT_LIST", XA_WINDOW, &n);

	for (i = 0; windows != NULL && i < n; i++)
		if (class_matches(windows[i], name, NULL))
			result = windows[i];

	if (windows != NULL)
		XFree(windows);

	return result;
}

void
screenshot(const char *name)
{
	FILE *file = NULL;
	XImage *im = NULL;
	int h, w, y;
	char path[PATH_MAX + 32];

	snprintf(path, sizeof(path), "%s/%s.ppm", output_path, name);

	file = fopen(path, "wb");

	CHECK(file);

	w = DisplayWidth(dpy, DefaultScreen(dpy));
	h = DisplayHeight(dpy, DefaultScreen(dpy));
	im = XGetImage(dpy, root, 0, 0, (unsigned int)w, (unsigned int)h,
		AllPlanes, ZPixmap);

	CHECK(im);
	fprintf(file, "P6\n%d %d\n255\n", w, h);

	for (y = 0; y < h; y++) {
		int x;

		for (x = 0; x < w; x++) {
			unsigned long pixel;
			unsigned char rgb[3] = {0};

			pixel = XGetPixel(im, x, y);
			rgb[0] = (unsigned char)(pixel >> 16);
			rgb[1] = (unsigned char)(pixel >> 8);
			rgb[2] = (unsigned char)pixel;

			CHECK(fwrite(rgb, 1, 3, file) == 3);
		}
	}

	XDestroyImage(im);
	CHECK(fclose(file) == 0);
}

int
received_protocol(Window w, const char *protocol)
{
	int found = 0;

	while (XPending(dpy)) {
		XEvent e;

		XNextEvent(dpy, &e);

		if (e.type == ClientMessage && e.xclient.window == w &&
			e.xclient.message_type == atom("WM_PROTOCOLS") &&
			(Atom)e.xclient.data.l[0] == atom(protocol))
			found = 1;
	}

	return found;
}

static Window
find_named_window(const char *expected, int visible_only)
{
	Window *children = NULL, parent_return, result = None, root_return;
	unsigned int count, i;

	if (XQueryTree(dpy, root, &root_return, &parent_return, &children,
		&count) == 0)
		return None;

	for (i = 0; i < count; i++) {
		char *name = NULL;

		if (XFetchName(dpy, children[i], &name) && name != NULL &&
			strcmp(name, expected) == 0 &&
			(!visible_only ||
				attributes(children[i]).map_state ==
					IsViewable))
			result = children[i];

		if (name != NULL)
			XFree(name);
	}

	if (children != NULL)
		XFree(children);

	return result;
}

Window
named_popup(const char *expected)
{
	return find_named_window(expected, 1);
}

Window
desktop_menu(void)
{
	return named_popup("owm desktop menu");
}

void
right_click(int x, int y)
{
	fake_motion(dpy, DefaultScreen(dpy), x, y, 0);
	fake_button(dpy, Button3, True, 0);
	fake_button(dpy, Button3, False, 0);
	XFlush(dpy);
	pause_ms(100);
}

int
stack_position(Window window)
{
	Window *children = NULL, parent, r;
	int position;
	unsigned int count, i;

	CHECK(XQueryTree(dpy, root, &r, &parent, &children, &count));

	position = -1;

	for (i = 0; i < count; i++)
		if (children[i] == window)
			position = (int)i;

	if (children != NULL)
		XFree(children);
	CHECK(position >= 0);

	return position;
}

Window
taskbar_window(void)
{
	Window window;

	window = find_named_window("owm taskbar", 0);
	CHECK(window != None);

	return window;
}

Window
find_icon(const char *id)
{
	Window *children = NULL, parent, r, result = None;
	unsigned int count, i;
	CHECK(XQueryTree(dpy, root, &r, &parent, &children, &count));

	for (i = 0; i < count; i++)
		if (class_matches(children[i], id, "OwmDesktopIcon"))
			result = children[i];

	if (children != NULL)
		XFree(children);

	return result;
}

void
type_command(const char *text)
{
	for (; (*text != '\0'); text++)
		shortcut((KeySym)(unsigned char)*text, 0);
}

void
check_fullscreen(Window window)
{
	XWindowAttributes client, frame;
	unsigned long *extents = NULL, n;

	WAIT(state(window, "_NET_WM_STATE_FULLSCREEN"));

	frame = attributes(parent_of(window));
	client = attributes(window);

	CHECK(frame.x == 0 && frame.y == 0 && frame.width == 1280 &&
		frame.height == 800);
	CHECK(client.x == 0 && client.y == 0 && client.width == 1280 &&
		client.height == 800);

	extents = property(window, "_NET_FRAME_EXTENTS", XA_CARDINAL, &n);

	CHECK(extents && n == 4 && !extents[0] && !extents[1] && !extents[2] &&
		!extents[3]);

	XFree(extents);
}
