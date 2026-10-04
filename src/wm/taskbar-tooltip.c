#include <stdlib.h>
#include <string.h>

#include "clock.h"
#include "memory.h"
#include "tray-widgets.h"
#include "utf8.h"
#include "wm.h"

#define TOOLTIP_DELAY_SECONDS 0.5

enum {
	TOOLTIP_PADDING_X = 8,
	TOOLTIP_PADDING_Y = 4,
	TOOLTIP_BAR_GAP = 4
};

struct tooltip_line {
	size_t	offset;
	int	length;
};

static Window popup, owner;
static char *hovered = NULL;
static struct tooltip_line *lines = NULL;
static double deadline;
static int anchor, width, height, line_count;
static int visible, screen_width, screen_height;
static int pointer_x, pointer_y;
static unsigned int pointer_state;

static void		 hide(void);
static void		 draw(void);
static const char	*text_at(int, int, Window *);
static void		 hover(int, int, unsigned int);
static int		 refresh_hover(void);
static void		 layout(void);
static void		 show(void);

static void
hide(void)
{
	if (visible)
		XUnmapWindow(wm.display, popup);
	visible = 0;
	free(hovered);
	free(lines);
	hovered = NULL;
	lines = NULL;
	owner = None;
}

static void
draw(void)
{
	XRectangle clip;
	int i, line_height;

	if (!visible || hovered == NULL)
		return;

	wm_draw_box(popup, 0, 0, width, height, 0);
	clip.x = TOOLTIP_PADDING_X;
	clip.y = TOOLTIP_PADDING_Y;
	clip.width = (unsigned short)(width - 2 * TOOLTIP_PADDING_X);
	clip.height = (unsigned short)(height - 2 * TOOLTIP_PADDING_Y);
	line_height = wm.font->ascent + wm.font->descent;
	for (i = 0; i < line_count; i++) {
		if (i == line_count - 1 &&
			hovered[lines[i].offset + lines[i].length] != '\0') {
			wm_draw_text(popup, TOOLTIP_PADDING_X,
				TOOLTIP_PADDING_Y + i * line_height,
				width - 2 * TOOLTIP_PADDING_X, line_height,
				hovered + lines[i].offset, wm.colors[BLACK]);
			return;
		}
		font_draw(wm.font, popup, TOOLTIP_PADDING_X,
			TOOLTIP_PADDING_Y + i * line_height + wm.font->ascent,
			hovered + lines[i].offset, lines[i].length,
			wm.colors[BLACK], &clip);
	}
}

static const char *
text_at(int x, int y, Window *target)
{
	struct client *c = NULL;

	c = taskbar_client_at(x, y);
	*target = c != NULL ? c->window : None;
	if (c != NULL)
		return c->title;

	return tray_widgets_tooltip(x, y);
}

static void
hover(int x, int y, unsigned int state)
{
	Window target;
	const char *text = NULL;

	text = text_at(x, y, &target);
	if (state &
		(Button1Mask | Button2Mask | Button3Mask | Button4Mask |
			Button5Mask))
		text = NULL;

	if (text == NULL || *text == '\0') {
		hide();
		return;
	}
	if (hovered != NULL && owner == target && strcmp(text, hovered) == 0)
		return;

	hide();
	/* Titles can change or disappear while the popup is waiting to open. */
	hovered = copy_string(text);
	owner = target;
	anchor = x;
	deadline = monotonic_time() + TOOLTIP_DELAY_SECONDS;
}

int
taskbar_tooltip_event(XEvent *event)
{
	if (event->type == Expose && popup && event->xany.window == popup) {
		if (event->xexpose.count == 0)
			draw();
		return 1;
	}

	if (event->type == ButtonPress || event->type == KeyPress)
		hide();
	if (event->xany.window != wm.taskbar)
		return 0;

	switch (event->type) {
	case LeaveNotify:
		hide();
		break;
	case EnterNotify:
		if (event->xcrossing.mode != NotifyNormal)
			break;
		pointer_x = event->xcrossing.x_root;
		pointer_y = event->xcrossing.y_root;
		pointer_state = event->xcrossing.state;
		hover(event->xcrossing.x, event->xcrossing.y, pointer_state);
		break;
	case MotionNotify:
		pointer_x = event->xmotion.x_root;
		pointer_y = event->xmotion.y_root;
		pointer_state = event->xmotion.state;
		hover(event->xmotion.x, event->xmotion.y, pointer_state);
		break;
	default:
		break;
	}

	return 0;
}

/* Check for stale crossings once, just before opening the popup. */
static int
refresh_hover(void)
{
	Window child, root;
	int root_x, root_y, window_x, window_y;
	unsigned int state;

	if (XQueryPointer(wm.display, wm.root, &root, &child, &root_x, &root_y,
		&window_x, &window_y, &state) == 0 ||
		child != wm.taskbar)
		return 0;

	pointer_x = root_x;
	pointer_y = root_y;
	pointer_state = state;
	hover(root_x, root_y - desktop_work_height(), state);

	return hovered != NULL;
}

static void
layout(void)
{
	size_t offset = 0;
	int available, line_height, max_lines;

	line_height = wm.font->ascent + wm.font->descent;
	available = wm.width > 2 * TOOLTIP_PADDING_X
		? wm.width - 2 * TOOLTIP_PADDING_X
		: 1;
	max_lines = (desktop_work_height() - 2 * TOOLTIP_PADDING_Y -
			TOOLTIP_BAR_GAP) /
		line_height;
	if (max_lines < 1)
		max_lines = 1;
	free(lines);
	lines = allocate_array((size_t)max_lines, sizeof(*lines));
	line_count = 0;
	width = 2 * TOOLTIP_PADDING_X + 1;

	while (hovered[offset] && line_count < max_lines) {
		size_t end, next, space;
		int pixels;

		end = offset;
		space = offset;
		while (hovered[end]) {
			next = utf8_next(hovered, end);
			pixels = font_width(wm.font, hovered + offset,
				(int)(next - offset));
			if (pixels > available && end > offset) {
				if (space > offset)
					end = space;
				break;
			}
			end = next;
			if (hovered[end - 1] == ' ')
				space = end;
			if (pixels > available)
				break;
		}
		lines[line_count].offset = offset;
		lines[line_count++].length = (int)(end - offset);
		pixels = font_width(
			wm.font, hovered + offset, (int)(end - offset));
		if (pixels + 2 * TOOLTIP_PADDING_X > width)
			width = pixels + 2 * TOOLTIP_PADDING_X;
		offset = end;
	}
	if (width > wm.width)
		width = wm.width;
	height = line_count * line_height + 2 * TOOLTIP_PADDING_Y;
	screen_width = wm.width;
	screen_height = wm.height;
}

static void
show(void)
{
	int x, y;

	if (visible && screen_width == wm.width && screen_height == wm.height)
		return;
	layout();
	x = anchor - width / 2;
	if (x > wm.width - width)
		x = wm.width - width;
	if (x < 0)
		x = 0;
	y = desktop_work_height() - height - TOOLTIP_BAR_GAP;
	if (y < 0)
		y = 0;

	if (popup == 0) {
		XSetWindowAttributes attributes = {0};
		Atom type;

		attributes.override_redirect = True;
		attributes.background_pixel = wm.colors[FACE];
		attributes.event_mask = ExposureMask;
		popup = XCreateWindow(wm.display, wm.root, x, y, width, height,
			0, CopyFromParent, InputOutput, CopyFromParent,
			CWOverrideRedirect | CWBackPixel | CWEventMask,
			&attributes);
		XStoreName(wm.display, popup, "owm taskbar tooltip");
		type = wm.atoms[A_TYPE_TOOLTIP];
		XChangeProperty(wm.display, popup, wm.atoms[A_TYPE], XA_ATOM,
			32, PropModeReplace, (unsigned char *)&type, 1);
	}

	XMoveResizeWindow(wm.display, popup, x, y, width, height);
	XMapRaised(wm.display, popup);
	visible = 1;
	draw();
}

void
taskbar_tooltip_update(double now)
{
	if (hovered == NULL)
		return;
	/* Recheck changed titles and layouts using the latest pointer event. */
	hover(pointer_x, pointer_y - desktop_work_height(), pointer_state);
	if (hovered == NULL || now < deadline)
		return;
	if (!visible) {
		if (!refresh_hover()) {
			hide();
			return;
		}
		if (now < deadline)
			return;
	}
	show();
}

double
taskbar_tooltip_deadline(void)
{
	return hovered != NULL && !visible ? deadline : 0;
}

void
taskbar_tooltip_free(void)
{
	hide();
	if (popup)
		wm_draw_destroy_window(popup);
	popup = None;
}
