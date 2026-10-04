#include <X11/cursorfont.h>
#include <stdlib.h>
#include <string.h>

#include "wm.h"

enum {
	EDGE_LEFT = 1,
	EDGE_RIGHT = 2,
	EDGE_TOP = 4,
	EDGE_BOTTOM = 8
};

enum {
	CAP_NONE,
	CAP_MIN,
	CAP_MAX,
	CAP_MENU
};

struct drag {
	struct client	*client;
	int		mode;
	int		edges;
	int		caption;
	int		start_x;
	int		start_y;
	struct geometry	start;
	int		inside;
	int		moved;
};

enum {
	DRAG_NONE,
	DRAG_MOVE,
	DRAG_RESIZE,
	DRAG_CAPTION
};

static struct drag drag;
static Cursor move_cursor, resize_cursor[16];
static Window last_click_window;
static Time last_click_time;
static int last_click_x, last_click_y;

static void	color(unsigned long);
static void	line(Window, int, int, int, int);
static int	caption_width(void);
static int	caption_height(void);
static int	caption_y(void);
static int	caption_x(struct client *, int);
static void	caption_glyph(struct client *, int, int, int, int);
static int	first_caption(const struct client *);
static int	caption_at(struct client *, int, int);
static int	edge_at(struct client *, int, int);
static void	activate_caption(struct client *, int, Time);

int
window_frame_top(void)
{
	return BORDER + wm.config.titlebar_height;
}

int
window_frame_width(const struct client *c)
{
	return c->g.w + (c->fullscreen ? 0 : 2 * BORDER);
}

int
window_frame_height(const struct client *c)
{
	return c->g.h + (c->fullscreen ? 0 : window_frame_top() + BORDER);
}

static void
color(unsigned long pixel)
{
	XSetForeground(wm.display, wm.gc, pixel);
}

static void
line(Window w, int x1, int y1, int x2, int y2)
{
	XDrawLine(wm.display, w, wm.gc, x1, y1, x2, y2);
}

void
window_frame_init(void)
{
	wm.normal_cursor = XCreateFontCursor(wm.display, XC_left_ptr);
	move_cursor = XCreateFontCursor(wm.display, XC_fleur);
	resize_cursor[EDGE_LEFT] = XCreateFontCursor(wm.display, XC_left_side);
	resize_cursor[EDGE_RIGHT] =
		XCreateFontCursor(wm.display, XC_right_side);
	resize_cursor[EDGE_TOP] = XCreateFontCursor(wm.display, XC_top_side);
	resize_cursor[EDGE_BOTTOM] =
		XCreateFontCursor(wm.display, XC_bottom_side);
	resize_cursor[EDGE_LEFT | EDGE_TOP] =
		XCreateFontCursor(wm.display, XC_top_left_corner);
	resize_cursor[EDGE_RIGHT | EDGE_TOP] =
		XCreateFontCursor(wm.display, XC_top_right_corner);
	resize_cursor[EDGE_LEFT | EDGE_BOTTOM] =
		XCreateFontCursor(wm.display, XC_bottom_left_corner);
	resize_cursor[EDGE_RIGHT | EDGE_BOTTOM] =
		XCreateFontCursor(wm.display, XC_bottom_right_corner);
	XDefineCursor(wm.display, wm.root, wm.normal_cursor);
}

void
window_frame_free(void)
{
	int i;

	window_frame_cancel_drag(NULL);
	window_frame_reset_click();
	for (i = 0; i < 16; i++) {
		if (resize_cursor[i])
			XFreeCursor(wm.display, resize_cursor[i]);
		resize_cursor[i] = None;
	}

	if (move_cursor)
		XFreeCursor(wm.display, move_cursor);
	if (wm.normal_cursor)
		XFreeCursor(wm.display, wm.normal_cursor);
	move_cursor = wm.normal_cursor = None;
}

static int
caption_width(void)
{
	return wm.config.titlebar_height;
}

static int
caption_height(void)
{
	return wm.config.titlebar_height;
}

static int
caption_y(void)
{
	return BORDER;
}

static int
caption_x(struct client *c, int caption)
{
	if (caption == CAP_MENU)
		return BORDER;

	return window_frame_width(c) - BORDER -
		(caption == CAP_MIN ? 2 : 1) * caption_width();
}

static void
caption_glyph(struct client *c, int caption, int x, int y, int pressed)
{
	int left, size, top;

	if (caption == CAP_MENU) {
		int height, width;

		width = caption_width() / 2;
		height = caption_height() / 5;

		if (height < 4)
			height = 4;
		wm_draw_box(c->frame,
			x + (caption_width() - width) / 2 + pressed,
			y + (caption_height() - height) / 2 + pressed, width,
			height, 0);

		return;
	}

	if (c->dialog) {
		int width;

		width = font_width(wm.font, "?", 1);

		wm_draw_text(c->frame,
			x + (caption_width() - width) / 2 + pressed,
			y + pressed, width, caption_height(), "?",
			wm.colors[BLACK]);

		return;
	}

	size = caption_width() / (caption == CAP_MIN ? 4 : 2);

	if (size < 5)
		size = 5;

	left = x + (caption_width() - size) / 2 + pressed;
	top = y + (caption_height() - size) / 2 + pressed;

	wm_draw_box(c->frame, left, top, size, size,
		caption == CAP_MAX && (c->max_h || c->max_v));

	return;
}

static int
first_caption(const struct client *c)
{
	if (!c->dialog)
		return CAP_MIN;

	return c->context_help ? CAP_MAX : CAP_MENU;
}

void
window_frame_draw(struct client *c)
{
	int available, cap, first, h, i, left, right, text_width, text_x, title,
		w;
	int corners[2] = {0};
	int joints[2] = {0};

	if (c->frame == 0 || c->fullscreen)
		return;

	w = window_frame_width(c);
	h = window_frame_height(c);
	title = wm.config.titlebar_height;

	wm_draw_box(c->frame, 0, 0, w, h, 0);

	color(wm.colors[SHADOW]);
	line(c->frame, BORDER - 1, BORDER - 1, w - BORDER, BORDER - 1);
	line(c->frame, BORDER - 1, BORDER - 1, BORDER - 1, h - BORDER);
	color(wm.colors[LIGHT]);
	line(c->frame, BORDER - 1, h - BORDER, w - BORDER, h - BORDER);
	line(c->frame, w - BORDER, BORDER - 1, w - BORDER, h - BORDER);

	first = first_caption(c);

	left = BORDER + caption_width();
	right = first == CAP_MENU ? w - BORDER : caption_x(c, first);

	wm_draw_surface(c->frame, left, BORDER, right - left, title,
		wm.colors[c == wm.focused ? ACTIVE : FACE], 0);

	available = right - left - 8;
	text_width = font_width(wm.font, c->title, (int)strlen(c->title));
	text_x = left + 4;

	if (text_width < available)
		text_x += (available - text_width) / 2;
	wm_draw_title(c->frame, text_x, BORDER, right - text_x - 4, title,
		c->title, wm.colors[c == wm.focused ? WHITE : BLACK]);

	/*
	 * Continue the adjoining buttons' bevels through the frame.
	 * The shadow ends before the boundary. The highlight starts on it.
	 */
	corners[0] = caption_x(c, CAP_MENU) + caption_width();
	corners[1] = caption_x(c, CAP_MAX);

	for (i = 0; i < 2; i++) {
		int offset;

		for (offset = -2; offset < 2; offset++) {
			int x;

			x = corners[i] + offset;

			color(wm.colors[offset < 0 ? SHADOW : LIGHT]);
			line(c->frame, x, 1, x, BORDER - 1);
			line(c->frame, x, h - BORDER, x, h - 2);
		}
	}

	joints[0] = window_frame_top();
	joints[1] = h - BORDER - caption_height();

	for (i = 0; i < 2; i++) {
		int offset;

		for (offset = -2; offset < 2; offset++) {
			int y;

			y = joints[i] + offset;

			color(wm.colors[offset < 0 ? SHADOW : LIGHT]);
			line(c->frame, 1, y, BORDER - 1, y);
			line(c->frame, w - BORDER, y, w - 2, y);
		}
	}

	for (cap = first; cap <= CAP_MENU; cap++) {
		int pressed, x, y;

		x = caption_x(c, cap);
		y = caption_y();
		pressed = drag.client == c && drag.mode == DRAG_CAPTION &&
			drag.caption == cap && drag.inside;

		wm_draw_box(c->frame, x, y, caption_width(), caption_height(),
			pressed);
		caption_glyph(c, cap, x, y, pressed);
	}

	color(wm.colors[SHADOW]);
	line(c->frame, BORDER - 1, window_frame_top() - 1, w - BORDER,
		window_frame_top() - 1);
}

static int
caption_at(struct client *c, int x, int y)
{
	int cap;

	if (y < caption_y() || y >= caption_y() + caption_height())
		return CAP_NONE;

	for (cap = first_caption(c); cap <= CAP_MENU; cap++)
		if (x >= caption_x(c, cap) &&
			x < caption_x(c, cap) + caption_width())
			return cap;

	return CAP_NONE;
}

static int
edge_at(struct client *c, int x, int y)
{
	int edges = 0, h, w;

	w = window_frame_width(c);
	h = window_frame_height(c);

	if (x < 0 || y < 0 || x >= w || y >= h)
		return 0;

	if (x >= BORDER && x < w - BORDER && y >= BORDER && y < h - BORDER)
		return 0;

	if (x < 12)
		edges |= EDGE_LEFT;

	if (x >= w - 12)
		edges |= EDGE_RIGHT;

	if (y < 12)
		edges |= EDGE_TOP;

	if (y >= h - 12)
		edges |= EDGE_BOTTOM;

	return edges;
}

int
window_frame_drag_active(void)
{
	return drag.mode != DRAG_NONE;
}

int
window_frame_resizing(const struct client *c)
{
	return drag.client == c && drag.mode == DRAG_RESIZE;
}

void
window_frame_cancel_drag(const struct client *c)
{
	if (c != NULL && drag.client != c)
		return;
	if (drag.mode)
		XUngrabPointer(wm.display, CurrentTime);
	memset(&drag, 0, sizeof(drag));
}

void
window_frame_reset_click(void)
{
	last_click_window = None;
}

void
window_frame_forget(const struct client *c)
{
	window_frame_cancel_drag(c);
	if (last_click_window == c->window)
		window_frame_reset_click();
}

void
window_frame_press(struct client *c, XButtonEvent *e)
{
	Cursor cursor;
	int caption, double_click, edges, mode;

	if (c->fullscreen || e->button != Button1 || drag.mode)
		return;

	edges = edge_at(c, e->x, e->y);
	caption = caption_at(c, e->x, e->y);
	mode = DRAG_NONE;

	if (edges)
		mode = DRAG_RESIZE;
	else if (caption)
		mode = DRAG_CAPTION;
	else if (e->y < window_frame_top())
		mode = DRAG_MOVE;

	double_click = mode == DRAG_MOVE && last_click_window == c->window &&
		(unsigned int)(e->time - last_click_time) <=
			(unsigned int)wm.config.double_click_ms &&
		abs(e->x_root - last_click_x) <= 4 &&
		abs(e->y_root - last_click_y) <= 4;

	last_click_window = None;

	if (double_click) {
		int maximize;

		maximize = !(c->max_h || c->max_v);

		window_client_maximize(c, maximize, maximize);

		return;
	}

	if (!mode || (mode == DRAG_RESIZE && (c->max_h || c->max_v)))
		return;

	cursor = wm.normal_cursor;

	if (mode == DRAG_RESIZE)
		cursor = resize_cursor[edges];
	else if (mode == DRAG_MOVE)
		cursor = move_cursor;

	if (XGrabPointer(wm.display, wm.root, False,
		ButtonReleaseMask | PointerMotionMask, GrabModeAsync,
		GrabModeAsync, None, cursor, e->time) != GrabSuccess)
		return;

	drag.client = c;
	drag.mode = mode;
	drag.edges = edges;
	drag.caption = caption;
	drag.start_x = e->x_root;
	drag.start_y = e->y_root;
	drag.start = c->g;
	drag.inside = 1;
	drag.moved = 0;
	window_frame_draw(c);
}

void
window_frame_motion(XMotionEvent *e)
{
	struct client *c = NULL;
	struct geometry g;
	int dx, dy;

	c = drag.client;

	if (c == NULL) {
		c = window_client_find(e->window);

		if (c != NULL && e->window == c->frame) {
			Cursor cursor;
			int edges;

			edges = c->max_h || c->max_v ? 0
						: edge_at(c, e->x, e->y);
			cursor =
				edges ? resize_cursor[edges] : wm.normal_cursor;

			if (cursor != c->frame_cursor) {
				XDefineCursor(wm.display, c->frame, cursor);
				c->frame_cursor = cursor;
			}
		}

		return;
	}

	dx = e->x_root - drag.start_x;
	dy = e->y_root - drag.start_y;

	if (drag.mode == DRAG_CAPTION) {
		int inside;

		inside = caption_at(c, e->x_root - c->g.x,
				e->y_root - c->g.y) == drag.caption;

		if (inside != drag.inside) {
			drag.inside = inside;
			window_frame_draw(c);
		}

		return;
	}

	if (!drag.moved && abs(dx) <= 4 && abs(dy) <= 4)
		return;

	drag.moved = 1;
	last_click_window = None;

	if (drag.mode == DRAG_MOVE && (c->max_h || c->max_v)) {
		double fraction;

		if (abs(dx) + abs(dy) < 4)
			return;

		fraction =
			(double)(drag.start_x - c->g.x) / window_frame_width(c);

		window_client_maximize(c, 0, 0);
		drag.start = c->g;
		drag.start.x =
			e->x_root - (int)(fraction * window_frame_width(c));
		drag.start.y =
			e->y_root - BORDER - wm.config.titlebar_height / 2;
		drag.start_x = e->x_root;
		drag.start_y = e->y_root;
		dx = dy = 0;
	}

	g = drag.start;

	if (drag.mode == DRAG_MOVE) {
		g.x += dx;
		g.y += dy;

		window_client_constrain_move(&g);
	} else {
		int edges;

		edges = drag.edges;

		if (edges & EDGE_LEFT)
			g.w -= dx;

		if (edges & EDGE_RIGHT)
			g.w += dx;

		if (edges & EDGE_TOP)
			g.h -= dy;

		if (edges & EDGE_BOTTOM)
			g.h += dy;
		window_client_constrain_size(c, &g.w, &g.h);

		if (edges & EDGE_LEFT)
			g.x += drag.start.w - g.w;

		if (edges & EDGE_TOP)
			g.y += drag.start.h - g.h;
	}

	window_client_configure(c, g, 0);
}

static void
activate_caption(struct client *c, int caption, Time time)
{
	int maximize;

	if (caption == CAP_MENU) {
		owm_menu_open_window(c, c->g.x + BORDER,
			c->g.y + window_frame_top(), time, 0);
		return;
	}

	if (caption == CAP_MIN) {
		window_client_minimize(c, 1);
		return;
	}

	if (caption != CAP_MAX)
		return;

	if (!c->dialog) {
		maximize = !(c->max_h || c->max_v);
		window_client_maximize(c, maximize, maximize);
		return;
	}

	wm_protocol_send(c, wm.atoms[A_CONTEXT_HELP], time);
}

void
window_frame_release(XButtonEvent *e)
{
	struct client *c = NULL;
	int activate, cap;

	if (e->button != Button1 || drag.client == NULL)
		return;

	c = drag.client;
	cap = drag.mode == DRAG_CAPTION ? drag.caption : CAP_NONE;
	activate = cap &&
		caption_at(c, e->x_root - c->g.x, e->y_root - c->g.y) == cap;

	if (drag.mode == DRAG_MOVE && !drag.moved) {
		last_click_window = c->window;
		last_click_time = e->time;
		last_click_x = e->x_root;
		last_click_y = e->y_root;
	}

	window_frame_cancel_drag(NULL);

	if (activate)
		activate_caption(c, cap, e->time);

	window_frame_draw(c);
}
