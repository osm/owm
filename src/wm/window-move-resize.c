#include <X11/keysym.h>
#include <string.h>

#include "settings.h"
#include "string-util.h"
#include "text-input.h"
#include "wm.h"

static struct {
	struct client		*client;
	struct window_state	saved;
	Window			hint;
	int			width;
	int			height;
	int			x;
	int			y;
	int			row;
	int			active;
	int			edited;
	int			invalid;
	struct ui		ui;
	struct text_input	coordinates[2];
	struct ui_rect		fields[2];
} moving;

static const char hint_controls[] =
	"Click/Tab: X/Y or arrow controls | Enter: accept | Esc: cancel";

static void	create_hint(void);
static void	draw_hint(void);
static int	resize_step(struct client *, int, int);
static void	sync_coordinates(void);
static int	apply_coordinates(void);
static void	handle_button(XButtonEvent *);
static void	cycle_field(int);
static void	edit_coordinate(KeySym, unsigned int, const char *, int);
static void	handle_arrow(KeySym, unsigned int);
static void	handle_key(XKeyEvent *);

int
window_move_active(void)
{
	return moving.client != NULL;
}

static void
sync_coordinates(void)
{
	int i;

	for (i = 0; i < 2; i++) {
		struct text_input *entry = NULL;

		entry = &moving.coordinates[i];
		snprintf(entry->text, sizeof(entry->text), "%d",
			i ? moving.client->g.y : moving.client->g.x);
		entry->cursor = strlen(entry->text);
		entry->offset = 0;
		entry->selected = 0;
	}
	moving.edited = moving.invalid = 0;
}

static int
apply_coordinates(void)
{
	struct client *c = NULL;
	struct geometry g;
	int i, values[2];

	if (!moving.edited)
		return 1;

	for (i = 0; i < 2; i++) {
		if (parse_integer(moving.coordinates[i].text,
			-WM_GEOMETRY_LIMIT, WM_GEOMETRY_LIMIT, &values[i]))
			continue;
		moving.active = i;
		moving.invalid = 1;
		draw_hint();
		return 0;
	}

	c = moving.client;
	if (c->max_h || c->max_v)
		window_client_maximize(c, 0, 0);
	g = c->g;
	g.x = values[0];
	g.y = values[1];
	window_client_constrain_move(&g);
	window_client_configure(c, g, 0);
	XRaiseWindow(wm.display, moving.hint);
	sync_coordinates();

	return 1;
}

static void
draw_hint(void)
{
	struct canvas canvas = {0};
	struct geometry g;
	int i;
	char geometry[128];

	g = moving.client->g;
	canvas.ui = &moving.ui;
	canvas.window = moving.hint;
	if (moving.invalid)
		snprintf(geometry, sizeof(geometry),
			"Enter whole-number X/Y coordinates (%d to %d).",
			-WM_GEOMETRY_LIMIT, WM_GEOMETRY_LIMIT);
	else
		snprintf(geometry, sizeof(geometry),
			"Resize/Move: %d,%d  %d x %d", g.x, g.y, g.w, g.h);
	wm_draw_box(moving.hint, 0, 0, moving.width, moving.height, 0);
	wm_draw_text(moving.hint, 8, 6, moving.width - 16, moving.row, geometry,
		wm.colors[BLACK]);
	for (i = 0; i < 2; i++) {
		struct ui_rect r;

		r = moving.fields[i];
		wm_draw_text(moving.hint, r.x - 24, r.y, 24, r.height,
			i ? "Y:" : "X:", wm.colors[BLACK]);
		ui_rect(canvas, r.x, r.y, r.width, r.height, wm.colors[WHITE]);
		ui_bevel(canvas, r.x, r.y, r.width, r.height, 1);
		text_input_draw(
			canvas, r, &moving.coordinates[i], moving.active == i);
	}
	wm_draw_text(moving.hint, 8, 6 + 2 * moving.row, moving.width - 16,
		moving.row,
		"Arrows: move | Ctrl: resize | Shift: fine (outside fields)",
		wm.colors[BLACK]);
	wm_draw_text(moving.hint, 8, 6 + 3 * moving.row, moving.width - 16,
		moving.row, hint_controls, wm.colors[BLACK]);
}

void
window_move_finish(int restore)
{
	struct client *c = NULL;

	c = moving.client;

	if (c == NULL)
		return;

	moving.client = NULL;
	XUngrabKeyboard(wm.display, CurrentTime);
	XUngrabPointer(wm.display, CurrentTime);
	wm_draw_destroy_window(moving.hint);
	moving.hint = None;
	ui_free(&moving.ui);

	if (!restore)
		return;

	window_state_restore_move(c, &moving.saved);
}

void
window_move_forget(struct client *c)
{
	if (moving.client == c)
		window_move_finish(0);
}

static void
create_hint(void)
{
	XSetWindowAttributes attributes = {0};
	int i, y;

	moving.width =
		font_width(wm.font, hint_controls, (int)strlen(hint_controls)) +
		16;

	if (moving.width > wm.width)
		moving.width = wm.width;
	moving.row = wm.font->ascent + wm.font->descent + 10;
	moving.height = 4 * moving.row + 12;
	moving.active = -1;
	ui_borrow(&moving.ui, wm.display, wm.gc, wm.font, wm.colors);
	for (i = 0; i < 2; i++) {
		moving.fields[i].x = 32 + i * (moving.width / 2);
		moving.fields[i].y = 6 + moving.row;
		moving.fields[i].width = moving.width / 2 - 40;
		moving.fields[i].height = moving.row - 2;
	}
	sync_coordinates();

	y = (wm.height - moving.height) / 2;

	if (y < 0)
		y = 0;

	moving.x = (wm.width - moving.width) / 2;
	moving.y = y;
	attributes.override_redirect = True;
	attributes.background_pixel = wm.colors[FACE];
	attributes.event_mask = ExposureMask;

	moving.hint = XCreateWindow(wm.display, wm.root, moving.x, moving.y,
		moving.width, moving.height, 0, CopyFromParent, InputOutput,
		CopyFromParent, CWOverrideRedirect | CWBackPixel | CWEventMask,
		&attributes);
	XStoreName(wm.display, moving.hint, "owm resize/move");
	XMapRaised(wm.display, moving.hint);
	draw_hint();
}

int
window_move_begin(struct client *c, Time time)
{
	if (c == NULL || !window_client_visible(c) || c->fullscreen ||
		window_frame_drag_active() || window_move_active())
		return 0;

	owm_menu_close();
	window_client_focus(c, time);

	if (XGrabKeyboard(wm.display, wm.root, False, GrabModeAsync,
		GrabModeAsync, time) != GrabSuccess)
		return 0;

	if (XGrabPointer(wm.display, wm.root, False,
		ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
		GrabModeAsync, GrabModeAsync, None, wm.normal_cursor,
		time) != GrabSuccess) {
		XUngrabKeyboard(wm.display, time);
		return 0;
	}

	window_frame_reset_click();
	moving.client = c;
	window_state_snapshot(c, &moving.saved);

	create_hint();

	return 1;
}

static int
resize_step(struct client *c, int horizontal, int fine)
{
	int increment = 1;
	int step;

	if (c->hints.flags & PResizeInc)
		increment =
			horizontal ? c->hints.width_inc : c->hints.height_inc;

	if (increment < 1)
		increment = 1;

	if (increment > WM_GEOMETRY_LIMIT)
		increment = WM_GEOMETRY_LIMIT;

	step = fine ? 1 : wm.config.resize_step;

	/* Round up to whole cells so every key press can change a terminal. */

	return ((step + increment - 1) / increment) * increment;
}

static void
handle_button(XButtonEvent *event)
{
	int i;

	if (event->button != Button1)
		return;
	for (i = 0; i < 2; i++) {
		if (!ui_contains(moving.fields[i], event->x_root - moving.x,
			event->y_root - moving.y))
			continue;
		if (moving.active != i) {
			if (moving.active >= 0)
				moving.coordinates[moving.active].selected = 0;
			moving.coordinates[i].selected = 1;
		} else
			text_input_click(&moving.coordinates[i], wm.font,
				moving.fields[i], event->x_root - moving.x);
		moving.active = i;
		draw_hint();
		return;
	}
	if (!apply_coordinates())
		return;

	moving.active = -1;
	moving.coordinates[0].selected = moving.coordinates[1].selected = 0;
	draw_hint();
}

static void
cycle_field(int backward)
{
	int next;

	next = moving.active + (backward ? -1 : 1);
	if (next < -1)
		next = 1;
	if (next > 1)
		next = -1;
	if (next < 0 && !apply_coordinates())
		return;
	if (moving.active >= 0)
		moving.coordinates[moving.active].selected = 0;
	moving.active = next;
	if (next >= 0)
		moving.coordinates[next].selected = 1;
	draw_hint();
}

static void
edit_coordinate(KeySym key, unsigned int state, const char *text, int length)
{
	struct text_input *entry = NULL;
	char old_text[32];

	entry = &moving.coordinates[moving.active];
	string_copy(old_text, entry->text, sizeof(old_text));
	if (length > 0 &&
		(entry->selected ? 0 : strlen(entry->text)) + (size_t)length >=
			sizeof(old_text))
		return;
	text_input_key(entry, key, state, text, length);
	if (strcmp(old_text, entry->text)) {
		moving.edited = 1;
		moving.invalid = 0;
	}
	draw_hint();
}

static void
handle_arrow(KeySym key, unsigned int state)
{
	struct client *c = NULL;
	struct geometry g;
	int dx, dy, fine;

	c = moving.client;

	dx = 0;
	dy = 0;

	switch (key) {
	case XK_Left:
	case XK_KP_Left:
	case XK_KP_4:
		dx = -1;
		break;
	case XK_Right:
	case XK_KP_Right:
	case XK_KP_6:
		dx = 1;
		break;
	case XK_Up:
	case XK_KP_Up:
	case XK_KP_8:
		dy = -1;
		break;
	case XK_Down:
	case XK_KP_Down:
	case XK_KP_2:
		dy = 1;
		break;
	default:
		return;
	}

	if (c->max_h || c->max_v)
		window_client_maximize(c, 0, 0);

	g = c->g;
	fine = state & ShiftMask;

	if (state & ControlMask) {
		g.w += dx * resize_step(c, 1, fine);
		g.h += dy * resize_step(c, 0, fine);
		window_client_constrain_size(c, &g.w, &g.h);
	} else {
		int step;

		step = fine ? 1 : wm.config.move_step;

		g.x += dx * step;
		g.y += dy * step;

		window_client_constrain_move(&g);
	}

	window_client_configure(c, g, 0);
	XRaiseWindow(wm.display, moving.hint);
	sync_coordinates();
	draw_hint();
}

static void
handle_key(XKeyEvent *event)
{
	KeySym key;
	int length;
	char text[32];

	length = XLookupString(event, text, sizeof(text), &key, NULL);

	switch (key) {
	case XK_Escape:
		window_move_finish(1);
		return;
	case XK_Return:
	case XK_KP_Enter:
		if (apply_coordinates())
			window_move_finish(0);
		return;
	case XK_Tab:
	case XK_ISO_Left_Tab:
		cycle_field(event->state & ShiftMask);
		return;
	default:
		break;
	}

	if (moving.active >= 0) {
		edit_coordinate(key, event->state, text, length);
		return;
	}

	handle_arrow(key, event->state);
}

int
window_move_event(XEvent *e)
{
	struct client *c = NULL;

	c = moving.client;

	if (c == NULL)
		return 0;

	if (!window_client_visible(c) || wm.focused != c) {
		window_move_finish(1);
		return 0;
	}

	if (e->type == ClientMessage && e->xclient.window == c->window &&
		(e->xclient.message_type == wm.atoms[A_NET_STATE] ||
			e->xclient.message_type == wm.atoms[A_CHANGE_STATE] ||
			e->xclient.message_type == wm.atoms[A_WM_DESKTOP] ||
			e->xclient.message_type == wm.atoms[A_CLOSE])) {
		window_move_finish(1);
		return 0;
	}

	if (e->type == Expose && e->xexpose.window == moving.hint) {
		if (e->xexpose.count == 0)
			draw_hint();
		return 1;
	}

	if (e->type == ButtonPress) {
		handle_button(&e->xbutton);
		return 1;
	}

	if (e->type == ButtonRelease || e->type == MotionNotify ||
		e->type == KeyRelease)
		return 1;

	if (e->type != KeyPress)
		return 0;

	handle_key(&e->xkey);

	return 1;
}
