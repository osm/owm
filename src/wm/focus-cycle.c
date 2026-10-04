#include <X11/keysym.h>

#include "wm.h"

static struct client *history = NULL;
static unsigned int modifiers;
static Window original;
static Window popup;

static int	released_modifier(XKeyEvent *);

void
focus_cycle_add(struct client *c)
{
	c->focus_next = history;
	history = c;
}

void
focus_cycle_forget(struct client *c)
{
	struct client **p = NULL;

	for (p = &history; *p != NULL; p = &(*p)->focus_next) {
		if (*p != c)
			continue;
		*p = c->focus_next;
		c->focus_next = NULL;
		focus_cycle_draw();
		return;
	}
}

void
focus_cycle_record(struct client *c)
{
	const struct client *owner = NULL;
	struct client **p = NULL;

	if (c == NULL || modifiers != 0)
		return;

	owner = desktop_owner(c);
	for (p = &history; *p != NULL; p = &(*p)->focus_next) {
		if (*p != owner)
			continue;
		c = *p;
		*p = c->focus_next;
		c->focus_next = history;
		history = c;
		return;
	}
}

int
focus_cycle_active(void)
{
	return modifiers != 0;
}

void
focus_cycle_begin(unsigned int held, Time time)
{
	Window child, root;
	int root_x, root_y, x, y;
	unsigned int state;

	/*
	 * Shift controls direction. It does not keep a switching session open.
	 */
	held &= ~ShiftMask;
	if (held == 0 || modifiers != 0)
		return;

	if (XGrabKeyboard(wm.display, wm.root, False, GrabModeAsync,
		GrabModeAsync, time) != GrabSuccess)
		return;

	/*
	 * A quick tap may have released its modifier before the grab arrived.
	 */
	if (!XQueryPointer(wm.display, wm.root, &root, &child, &root_x, &root_y,
		&x, &y, &state) ||
		(state & held) != held) {
		XUngrabKeyboard(wm.display, CurrentTime);
		return;
	}

	modifiers = held;
	original = wm.focused != NULL ? wm.focused->window : None;
}

void
focus_cycle_draw(void)
{
	struct client *c = NULL;
	const struct client *selected = NULL;
	XSetWindowAttributes attrs = {0};
	int count, index, current, first, rows, row_height;
	int width, height, header_height, icon_size, padding, y, active;

	if (modifiers == 0)
		return;

	selected = desktop_owner(wm.focused);
	count = current = 0;
	for (c = history; c != NULL; c = c->focus_next) {
		if (!desktop_contains(c) || !window_client_is_task(c))
			continue;
		if (c == selected)
			current = count;
		count++;
	}
	if (count == 0) {
		focus_cycle_finish();
		return;
	}

	padding = taskbar_pixels(10);
	icon_size = taskbar_pixels(TASKBAR_ICON_SIZE);
	row_height = wm.config.taskbar_height + taskbar_pixels(8);
	header_height = wm.font->ascent + wm.font->descent + 2 * padding;
	width = taskbar_pixels(360);
	if (width > wm.width)
		width = wm.width;
	rows = (wm.height - header_height - 2 * padding) / row_height;
	if (rows > 8)
		rows = 8;
	if (rows < 1)
		rows = 1;
	if (rows > count)
		rows = count;
	height = header_height + rows * row_height + 2 * padding;
	first = current - rows / 2;
	if (first > count - rows)
		first = count - rows;
	if (first < 0)
		first = 0;

	if (popup == None) {
		attrs.override_redirect = True;
		attrs.background_pixel = wm.colors[FACE];
		attrs.event_mask = ExposureMask;
		popup = XCreateWindow(wm.display, wm.root, 0, 0, 1, 1, 0,
			CopyFromParent, InputOutput, CopyFromParent,
			CWOverrideRedirect | CWBackPixel | CWEventMask, &attrs);
		XStoreName(wm.display, popup, "owm window switcher");
	}
	XMoveResizeWindow(wm.display, popup, (wm.width - width) / 2,
		(wm.height - height) / 2, width, height);
	wm_draw_box(popup, 0, 0, width, height, 0);
	wm_draw_text(popup, 2 * padding, padding, width - 4 * padding,
		header_height - padding, "Windows", wm.colors[BLACK]);
	XSetForeground(wm.display, wm.gc, wm.colors[SHADOW]);
	XDrawLine(wm.display, popup, wm.gc, padding, header_height,
		width - padding - 1, header_height);
	index = 0;
	for (c = history; c != NULL; c = c->focus_next) {
		if (!desktop_contains(c) || !window_client_is_task(c))
			continue;
		if (index++ < first)
			continue;
		if (index > first + rows)
			break;
		y = header_height + padding + (index - first - 1) * row_height;
		active = c == selected;
		if (active) {
			XSetForeground(wm.display, wm.gc, wm.colors[LIGHT]);
			XFillRectangle(wm.display, popup, wm.gc, padding, y,
				width - 2 * padding, row_height);
			XSetForeground(wm.display, wm.gc, wm.colors[ACTIVE]);
			XFillRectangle(wm.display, popup, wm.gc, padding, y,
				taskbar_pixels(3), row_height);
		}
		taskbar_icon_draw(c, popup, 2 * padding,
			y + (row_height - icon_size) / 2, active);
		wm_draw_text(popup, 3 * padding + icon_size, y,
			width - 5 * padding - icon_size, row_height, c->title,
			wm.colors[BLACK]);
	}
	XMapRaised(wm.display, popup);
}

void
focus_cycle_finish(void)
{
	if (modifiers == 0)
		return;

	modifiers = 0;
	original = None;
	if (popup != None)
		XUnmapWindow(wm.display, popup);
	XUngrabKeyboard(wm.display, CurrentTime);
	focus_cycle_record(wm.focused);
}

void
focus_cycle_free(void)
{
	focus_cycle_finish();
	if (popup != None)
		wm_draw_destroy_window(popup);
	popup = None;
}

void
focus_cycle_step(int backwards, Time time)
{
	const struct client *owner = NULL;
	struct client *c = NULL, *candidate = NULL, *first = NULL;
	struct client *previous = NULL;
	int passed;

	owner = desktop_owner(wm.focused);
	passed = owner == NULL;

	for (c = history; c != NULL; c = c->focus_next) {
		if (!desktop_contains(c) || !window_client_is_task(c))
			continue;
		if (first == NULL)
			first = c;
		if (backwards && c == owner)
			candidate = previous;
		if (!backwards && passed && candidate == NULL)
			candidate = c;
		if (c == owner)
			passed = 1;
		previous = c;
	}

	if (candidate == NULL)
		candidate = backwards ? previous : first;
	if (candidate != NULL)
		window_client_focus(candidate, time);
}

static int
released_modifier(XKeyEvent *event)
{
	XModifierKeymap *map = NULL;
	int index, modifier, released = 0;

	map = XGetModifierMapping(wm.display);
	if (map == NULL)
		return 1;

	for (modifier = 0; modifier < 8; modifier++) {
		if (!(modifiers & (1U << modifier)))
			continue;
		for (index = 0; index < map->max_keypermod; index++)
			if (map->modifiermap[modifier * map->max_keypermod +
				index] == event->keycode)
				released = 1;
	}
	XFreeModifiermap(map);

	return released;
}

int
focus_cycle_event(XEvent *event)
{
	struct client *c = NULL;

	if (modifiers == 0)
		return 0;

	if (event->type == Expose && event->xexpose.window == popup) {
		if (event->xexpose.count == 0)
			focus_cycle_draw();
		return 1;
	}

	if (event->type == KeyRelease) {
		if (released_modifier(&event->xkey))
			focus_cycle_finish();
		return 1;
	}
	if (event->type == KeyPress &&
		XLookupKeysym(&event->xkey, 0) == XK_Escape) {
		c = window_client_find(original);
		if (c != NULL && desktop_contains(c))
			window_client_focus(c, event->xkey.time);
		focus_cycle_finish();
		return 1;
	}
	if (event->type == ButtonPress || event->type == MappingNotify)
		focus_cycle_finish();

	return 0;
}
