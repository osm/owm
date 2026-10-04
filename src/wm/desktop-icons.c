#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "builtin-icons.h"
#include "icon-positions.h"
#include "process.h"
#include "string-util.h"
#include "wm.h"

/*
 * Small children with override_redirect set of the root, always below
 * application frames.
 */
struct icon {
	Window		window;
	struct wm_image	image;
	int		origin_x;
	int		origin_y;
	int		selected;
	int		was_selected;
};

static struct icon icons[MAX_DESKTOP_ICONS];
static struct icon_position positions[MAX_DESKTOP_ICONS];
static int width, height, pressed = -1, last_click = -1;
static int press_x, press_y, click_x, click_y;
static Time click_time;
static int moved, selecting, additive, modified_click;
static Window selection_edges[4];

enum {
	IMAGE_AREA = 48,
	GAP = 8
};

static int image_size, image_area;

static int	fit(int, int, int);
static void	position(int);
static void	load_image(int);
static void	draw(int);
static int	snap(int, int, int);
static void	select_icon(int, int);
static void	hide_selection(void);
static void	selection_motion(int, int);
static void	drag_motion(int, int);
static void	finish_icon_press(XButtonEvent *);
static int	begin_selection(XButtonEvent *);
static void	press_icon(int, XButtonEvent *);

static int
fit(int value, int extent, int size)
{
	int limit;

	limit = extent > size + GAP ? extent - size - GAP : 0;

	return value < 0 ? 0 : value > limit ? limit : value;
}

static void
position(int i)
{
	XMoveWindow(wm.display, icons[i].window,
		fit(positions[i].x, wm.width, width),
		fit(positions[i].y, desktop_work_height(), height));
	XLowerWindow(wm.display, icons[i].window);
}

/*
 * Scale loaded artwork and transparency together, preserving small custom
 * images at the default font size.
 */

static void
load_image(int i)
{
	struct wm_image *image = NULL;
	const char *path = NULL;

	image = &icons[i].image;
	path = wm.config.icons[i].image;
	if (*path != '/') {
		image->pixmap = builtin_icon_pixmap(wm.display, wm.root,
			DefaultDepth(wm.display, wm.screen), image_size,
			builtin_icon_parse(wm.config.icons[i].image),
			wm.colors[DESKTOP], wm.colors);
		image->width = image->height = image_size;
		return;
	}
	if (!wm_image_load(image, path, wm.colors[DESKTOP], IMAGE_AREA)) {
		warnx("icon '%s': cannot use '%s'; using default",
			wm.config.icons[i].id, path);
		return;
	}
	if (image_size != 32)
		wm_image_resize(image, image->width * image_size / 32,
			image->height * image_size / 32);
}

static void
draw(int i)
{
	struct icon *icon = NULL;
	int text_width, x;
	const char *label = NULL;

	icon = &icons[i];

	XClearWindow(wm.display, icon->window);

	if (icon->image.pixmap) {
		int x, y;

		x = (width - (int)icon->image.width) / 2;
		y = (image_area - (int)icon->image.height) / 2;

		XSetClipMask(wm.display, wm.gc, icon->image.mask);
		XSetClipOrigin(wm.display, wm.gc, x, y);
		XCopyArea(wm.display, icon->image.pixmap, icon->window, wm.gc,
			0, 0, icon->image.width, icon->image.height, x, y);
		XSetClipMask(wm.display, wm.gc, None);
		XSetClipOrigin(wm.display, wm.gc, 0, 0);
	} else {
		builtin_icon_draw(wm.display, icon->window, wm.gc,
			(width - image_size) / 2, (image_area - image_size) / 2,
			image_size,
			builtin_icon_parse(wm.config.icons[i].image),
			wm.colors);
	}

	label = wm.config.icons[i].label;
	text_width = font_width(wm.font, label, (int)strlen(label));

	if (text_width > width - 8)
		text_width = width - 8;

	x = (width - text_width) / 2;

	if (icon->selected) {
		XSetForeground(wm.display, wm.gc, wm.colors[ACTIVE]);
		XFillRectangle(wm.display, icon->window, wm.gc, x - 3,
			image_area, text_width + 6, height - image_area);
	}

	wm_draw_text(icon->window, x, image_area, text_width,
		height - image_area, label, wm.colors[WHITE]);
}

static int
snap(int value, int extent, int size)
{
	int cell, last, step;

	step = size + GAP;

	if (extent < size + 2 * GAP)
		return 0;

	cell = (value - GAP + step / 2) / step;
	last = (extent - size - 2 * GAP) / step;

	if (cell < 0)
		cell = 0;

	if (cell > last)
		cell = last;

	return GAP + cell * step;
}

static void
select_icon(int i, int selected)
{
	if (icons[i].selected == selected)
		return;

	icons[i].selected = selected;
	draw(i);
}

static void
hide_selection(void)
{
	int i;

	for (i = 0; i < 4; i++)
		if (selection_edges[i])
			XUnmapWindow(wm.display, selection_edges[i]);
}

static void
selection_motion(int x, int y)
{
	int bottom, i, left, rh, right, rw, top;
	int rectangles[4][4] = {0};

	left = x < press_x ? x : press_x;
	top = y < press_y ? y : press_y;
	right = x > press_x ? x : press_x;
	bottom = y > press_y ? y : press_y;

	if (left < 0)
		left = 0;

	if (top < 0)
		top = 0;

	if (right >= wm.width)
		right = wm.width - 1;

	if (bottom >= desktop_work_height())
		bottom = desktop_work_height() - 1;

	rw = right - left + 1;
	rh = bottom - top + 1;
	rectangles[0][0] = left;
	rectangles[0][1] = top;
	rectangles[0][2] = rw;
	rectangles[0][3] = 1;
	rectangles[1][0] = left;
	rectangles[1][1] = bottom;
	rectangles[1][2] = rw;
	rectangles[1][3] = 1;
	rectangles[2][0] = left;
	rectangles[2][1] = top;
	rectangles[2][2] = 1;
	rectangles[2][3] = rh;
	rectangles[3][0] = right;
	rectangles[3][1] = top;
	rectangles[3][2] = 1;
	rectangles[3][3] = rh;

	for (i = 0; i < 4; i++) {
		if (selection_edges[i] == 0) {
			XSetWindowAttributes attributes = {0};

			attributes.override_redirect = True;
			attributes.background_pixel = wm.colors[WHITE];

			selection_edges[i] = XCreateWindow(wm.display, wm.root,
				0, 0, 1, 1, 0, CopyFromParent, InputOutput,
				CopyFromParent,
				CWOverrideRedirect | CWBackPixel, &attributes);
		}

		XMoveResizeWindow(wm.display, selection_edges[i],
			rectangles[i][0], rectangles[i][1], rectangles[i][2],
			rectangles[i][3]);
		XLowerWindow(wm.display, selection_edges[i]);
		XMapWindow(wm.display, selection_edges[i]);
	}

	for (i = 0; i < wm.config.icon_count; i++) {
		int hit, ix, iy;

		ix = fit(positions[i].x, wm.width, width);
		iy = fit(positions[i].y, desktop_work_height(), height);
		hit = ix < right && ix + width > left && iy < bottom &&
			iy + height > top;

		select_icon(i, hit || (additive && icons[i].was_selected));
	}
}

static void
drag_motion(int x, int y)
{
	int dx, dy, i, max_x, max_y, min_x, min_y;

	dx = x - press_x;
	dy = y - press_y;

	if (!moved && abs(dx) <= 4 && abs(dy) <= 4)
		return;

	moved = 1;
	last_click = -1;
	dx = snap(icons[pressed].origin_x + dx, wm.width, width) -
		icons[pressed].origin_x;
	dy = snap(icons[pressed].origin_y + dy, desktop_work_height(), height) -
		icons[pressed].origin_y;

	min_x = -32768;
	max_x = 32768;
	min_y = -32768;
	max_y = 32768;

	for (i = 0; i < wm.config.icon_count; i++) {
		int hi, lo;

		if (!icons[i].selected)
			continue;

		lo = snap(0, wm.width, width) - icons[i].origin_x;
		hi = snap(wm.width, wm.width, width) - icons[i].origin_x;

		if (lo > min_x)
			min_x = lo;

		if (hi < max_x)
			max_x = hi;
		lo = snap(0, desktop_work_height(), height) - icons[i].origin_y;
		hi = snap(desktop_work_height(), desktop_work_height(),
			height) -
			icons[i].origin_y;

		if (lo > min_y)
			min_y = lo;

		if (hi < max_y)
			max_y = hi;
	}

	if (dx < min_x)
		dx = min_x;

	if (dx > max_x)
		dx = max_x;

	if (dy < min_y)
		dy = min_y;

	if (dy > max_y)
		dy = max_y;

	for (i = 0; i < wm.config.icon_count; i++) {
		int x, y;

		if (!icons[i].selected)
			continue;

		x = icons[i].origin_x + dx;
		y = icons[i].origin_y + dy;

		if (positions[i].x == x && positions[i].y == y)
			continue;

		positions[i].x = x;
		positions[i].y = y;
		position(i);
	}
}

void
desktop_icons_cancel_drag(void)
{
	if (pressed >= 0 || selecting) {
		int i;

		XUngrabPointer(wm.display, CurrentTime);

		for (i = 0; i < wm.config.icon_count; i++) {
			if (selecting)
				select_icon(i, icons[i].was_selected);
			else if (icons[i].selected) {
				positions[i].x = icons[i].origin_x;
				positions[i].y = icons[i].origin_y;
				position(i);
			}
		}
	}

	hide_selection();
	selecting = 0;
	pressed = -1;
	last_click = -1;
}

void
desktop_icons_resize(void)
{
	int i;

	desktop_icons_cancel_drag();

	for (i = 0; i < wm.config.icon_count; i++)
		position(i);
}

void
desktop_icons_init(void)
{
	int font_height, i, rows;

	font_height = wm.font->ascent + wm.font->descent;

	/* The default 10pt font has a 17px line height at 96 DPI. */
	image_size = font_height * 32 / 17;
	if (image_size < 32)
		image_size = 32;
	if (image_size > 256)
		image_size = 256;
	image_area = IMAGE_AREA * image_size / 32;
	width = 96 * image_size / 32;

	for (i = 0; i < wm.config.icon_count; i++) {
		int label_width;
		const char *label = NULL;

		label = wm.config.icons[i].label;
		label_width =
			font_width(wm.font, label, (int)strlen(label)) + 8;

		if (width < label_width)
			width = label_width;
	}

	if (wm.width > 2 * GAP && width > wm.width - 2 * GAP)
		width = wm.width - 2 * GAP;
	height = image_area + font_height + 6;

	rows = (desktop_work_height() - GAP) / (height + GAP);

	if (rows < 1)
		rows = 1;

	for (i = 0; i < wm.config.icon_count; i++) {
		positions[i].x = GAP + i / rows * (width + GAP);
		positions[i].y = GAP + i % rows * (height + GAP);
	}

	icon_positions_load(&wm.config, positions);

	for (i = 0; i < wm.config.icon_count; i++) {
		positions[i].x = snap(positions[i].x, wm.width, width);
		positions[i].y =
			snap(positions[i].y, desktop_work_height(), height);
	}

	for (i = 0; i < wm.config.icon_count; i++) {
		XSetWindowAttributes a = {0};
		struct icon *icon = NULL;
		XClassHint klass = {0};
		Atom type;

		a.override_redirect = True;
		a.background_pixel = wm.colors[DESKTOP];
		a.event_mask = ExposureMask | ButtonPressMask |
			ButtonReleaseMask | PointerMotionMask;
		icon = &icons[i];

		icon->window = XCreateWindow(wm.display, wm.root, 0, 0, width,
			height, 0, CopyFromParent, InputOutput, CopyFromParent,
			CWOverrideRedirect | CWBackPixel | CWEventMask, &a);

		klass.res_name = wm.config.icons[i].id;
		klass.res_class = "OwmDesktopIcon";

		XSetClassHint(wm.display, icon->window, &klass);
		XStoreName(wm.display, icon->window, wm.config.icons[i].label);

		type = wm.atoms[A_TYPE_DESKTOP];

		XChangeProperty(wm.display, icon->window, wm.atoms[A_TYPE],
			XA_ATOM, 32, PropModeReplace, (unsigned char *)&type,
			1);
		load_image(i);
		position(i);
		XMapWindow(wm.display, icon->window);
	}
}

static void
finish_icon_press(XButtonEvent *event)
{
	int index, twice;

	drag_motion(event->x_root, event->y_root);

	index = pressed;

	pressed = -1;
	XUngrabPointer(wm.display, event->time);

	if (moved) {
		if (!icon_positions_save(&wm.config, positions))
			warn("save icon positions");
		return;
	}

	twice = !modified_click && last_click == index &&
		(unsigned int)(event->time - click_time) <=
			(unsigned int)wm.config.double_click_ms &&
		abs(event->x_root - click_x) <= 4 &&
		abs(event->y_root - click_y) <= 4;

	last_click = twice || modified_click ? -1 : index;
	click_time = event->time;
	click_x = event->x_root;
	click_y = event->y_root;

	if (twice &&
		process_shell(ConnectionNumber(wm.display),
			wm.config.icons[index].command) < 0)
		warn("launch desktop icon");
}

static int
begin_selection(XButtonEvent *event)
{
	int j;

	last_click = -1;
	if (event->button != Button1 || wm.config.icon_count == 0)
		return 0;

	additive = (event->state & (ShiftMask | ControlMask)) != 0;

	for (j = 0; j < wm.config.icon_count; j++) {
		icons[j].was_selected = icons[j].selected;
		if (!additive)
			select_icon(j, 0);
	}

	if (XGrabPointer(wm.display, wm.root, False,
		PointerMotionMask | ButtonReleaseMask, GrabModeAsync,
		GrabModeAsync, None, wm.normal_cursor,
		event->time) != GrabSuccess)
		return 1;

	selecting = 1;
	press_x = event->x_root;
	press_y = event->y_root;
	window_client_focus(NULL, event->time);

	return 1;
}

static void
press_icon(int i, XButtonEvent *event)
{
	int j;

	window_frame_reset_click();
	if (event->button == Button3) {
		desktop_icons_cancel_drag();
		owm_menu_open(event->x_root, event->y_root, event->time);
		return;
	}

	if (event->button != Button1 || pressed >= 0)
		return;

	modified_click = (event->state & (ControlMask | ShiftMask)) != 0;

	if (modified_click) {
		select_icon(i, !icons[i].selected);
		last_click = -1;

		if (!icons[i].selected)
			return;
	} else if (!icons[i].selected) {
		for (j = 0; j < wm.config.icon_count; j++)
			select_icon(j, j == i);
	}

	window_client_focus(NULL, event->time);

	if (XGrabPointer(wm.display, wm.root, False,
		PointerMotionMask | ButtonReleaseMask, GrabModeAsync,
		GrabModeAsync, None, wm.normal_cursor,
		event->time) != GrabSuccess)
		return;

	pressed = i;
	moved = 0;
	press_x = event->x_root;
	press_y = event->y_root;

	for (j = 0; j < wm.config.icon_count; j++) {
		icons[j].origin_x = positions[j].x;
		icons[j].origin_y = positions[j].y;
	}
}

int
desktop_icons_event(XEvent *event)
{
	int i;

	for (i = 0; i < wm.config.icon_count; i++)
		if (icons[i].window == event->xany.window)
			break;

	if (selecting && event->type == MotionNotify) {
		selection_motion(event->xmotion.x_root, event->xmotion.y_root);
		return 1;
	}

	if (selecting && event->type == ButtonRelease &&
		event->xbutton.button == Button1) {
		hide_selection();
		selecting = 0;
		XUngrabPointer(wm.display, event->xbutton.time);
		return 1;
	}

	if (pressed >= 0 && event->type == MotionNotify) {
		drag_motion(event->xmotion.x_root, event->xmotion.y_root);
		return 1;
	}

	if (pressed >= 0 && event->type == ButtonRelease &&
		event->xbutton.button == Button1) {
		finish_icon_press(&event->xbutton);
		return 1;
	}

	if (i == wm.config.icon_count) {
		if (event->type != ButtonPress ||
			event->xbutton.window != wm.root)
			return 0;
		return begin_selection(&event->xbutton);
	}

	if (event->type == Expose) {
		if (event->xexpose.count == 0)
			draw(i);
		return 1;
	}

	if (event->type != ButtonPress)
		return event->type == ButtonRelease ||
			event->type == MotionNotify;

	press_icon(i, &event->xbutton);

	return 1;
}

void
desktop_icons_free(void)
{
	int i;

	desktop_icons_cancel_drag();

	for (i = 0; i < 4; i++)
		if (selection_edges[i])
			wm_draw_destroy_window(selection_edges[i]);

	for (i = 0; i < wm.config.icon_count; i++) {
		wm_image_free(&icons[i].image);
		wm_draw_destroy_window(icons[i].window);
	}
}
