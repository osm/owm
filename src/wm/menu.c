#include <X11/keysym.h>
#include <string.h>

#include "actions.h"
#include "string-util.h"
#include "wm.h"

/*
 * The override_redirect flag keeps the popup out of client lists and saved
 * sessions.
 */
enum {
	PADDING = 3,
	SEPARATOR = 7
};

#define ROW_HEIGHT                                                             \
	(wm.font->ascent + wm.font->descent + 8 > 24                           \
			? wm.font->ascent + wm.font->descent + 8               \
			: 24)

struct menu {
	Window			window;
	struct menu_item	*items;
	int			count;
	int			selected;
	int			pressed;
	int			failed;
	int			first;
	int			x;
	int			y;
	int			width;
	int			height;
};

static struct menu main_menu, desktop_menu;
static Window target;
static int opened, desktops_open;
static struct menu_item window_items[] = {{"Resize/Move", "window_move_resize"},
	{"Maximize", "window_maximize"}, {"Minimize", "window_minimize"},
	{"Sticky", "window_sticky"}, {"Fullscreen", "window_fullscreen"},
	{"Send to desktop", "desktops"}, {"", ""}, {"Close", "window_close"}};
static struct menu_item desktop_items[MAX_DESKTOPS];

static int		 separator(struct menu *, int);
static int		 enabled(struct menu *, int);
static int		 row_height(struct menu *, int);
static int		 title_height(struct menu *);
static int		 row_y(struct menu *, int);
static int		 item_at(struct menu *, int, int);
static void		 choose(struct menu *, int);
static void		 draw_separator(struct menu *, int);
static void		 draw(struct menu *);
static void		 close_desktops(void);
static void		 prepare(struct menu *);
static void		 show(struct menu *, int, int, const char *);
static int		 desktop_entry(int);
static void		 open_desktops(int);
static void		 open_popup(int, int, Time);
static void		 activate(struct menu *, int, Time);
static int		 contains(struct menu *, int, int);
static struct menu	*menu_at(int, int);
static int		 handle_expose(XExposeEvent *);
static void		 handle_motion(XMotionEvent *);
static void		 handle_button_press(XButtonEvent *);
static void		 handle_button_release(XButtonEvent *);
static void		 handle_key(XKeyEvent *);

static int
separator(struct menu *m, int index)
{
	return (*m->items[index].action == '\0');
}

static int
enabled(struct menu *m, int index)
{
	struct client *c = NULL;

	c = target ? window_client_find(target) : NULL;

	return target == 0 ||
		(c != NULL &&
			(m != &main_menu || index != 0 || !c->fullscreen));
}

static int
row_height(struct menu *m, int index)
{
	return separator(m, index) ? SEPARATOR : ROW_HEIGHT;
}

static int
title_height(struct menu *m)
{
	return m == &main_menu && target == 0 ? ROW_HEIGHT + SEPARATOR : 0;
}

static int
row_y(struct menu *m, int index)
{
	int i, y;

	y = PADDING + title_height(m);

	for (i = m->first; i < index; i++)
		y += row_height(m, i);

	return y;
}

static int
item_at(struct menu *m, int x, int y)
{
	int i;

	if (x < PADDING || x >= m->width - PADDING || y >= m->height - PADDING)
		return -1;

	for (i = m->first; i < m->count; i++)
		if (!separator(m, i) && enabled(m, i) &&
			row_y(m, i) + ROW_HEIGHT <= m->height - PADDING &&
			y >= row_y(m, i) && y < row_y(m, i) + ROW_HEIGHT)
			return i;

	return -1;
}

static void
choose(struct menu *m, int direction)
{
	int count, i;

	count = m->count;

	for (i = 0; i < count; i++) {
		m->selected = (m->selected + direction + count) % count;

		if (!separator(m, m->selected) && enabled(m, m->selected))
			break;
	}

	if (m->selected < m->first)
		m->first = m->selected;

	while (m->first < m->selected &&
		row_y(m, m->selected) + ROW_HEIGHT > m->height - PADDING)
		m->first++;
}

static void
draw_separator(struct menu *m, int y)
{
	XSetForeground(wm.display, wm.gc, wm.colors[SHADOW]);
	XDrawLine(wm.display, m->window, wm.gc, 4, y, m->width - 5, y);
	XSetForeground(wm.display, wm.gc, wm.colors[WHITE]);
	XDrawLine(wm.display, m->window, wm.gc, 4, y + 1, m->width - 5, y + 1);
}

static void
draw(struct menu *m)
{
	int i;

	wm_draw_box(m->window, 0, 0, m->width, m->height, 0);

	if (m == &main_menu && target == 0) {
		int title_width;

		title_width = font_width(wm.font, "owm", 3);

		wm_draw_text(m->window, (m->width - title_width) / 2, PADDING,
			title_width, ROW_HEIGHT, "owm", wm.colors[BLACK]);
		draw_separator(m, PADDING + ROW_HEIGHT + 3);
	}

	for (i = m->first; i < m->count; i++) {
		struct client *c = NULL;
		int active, inset;
		const char *text = NULL;

		if (row_y(m, i) + row_height(m, i) > m->height - PADDING)
			break;

		if (separator(m, i)) {
			draw_separator(m, row_y(m, i) + 3);
			continue;
		}

		active = i == m->selected;

		if (active) {
			wm_draw_surface(m->window, PADDING, row_y(m, i),
				m->width - 2 * PADDING, ROW_HEIGHT,
				wm.colors[ACTIVE], 0);
		}

		text = i == m->failed ? "Action failed; try again"
				: m->items[i].label;

		if (m == &main_menu && target &&
			strcmp(m->items[i].action, "desktops") == 0) {
			int x, y;
			XPoint arrow[3] = {0};

			x = m->width - 14;
			y = row_y(m, i) + ROW_HEIGHT / 2;
			arrow[0].x = x;
			arrow[0].y = y - 4;
			arrow[1].x = x + 4;
			arrow[1].y = y;
			arrow[2].x = x;
			arrow[2].y = y + 4;

			XSetForeground(wm.display, wm.gc,
				wm.colors[active ? WHITE : BLACK]);
			XFillPolygon(wm.display, m->window, wm.gc, arrow, 3,
				Convex, CoordModeOrigin);
		}

		inset = m == &desktop_menu ? 28 : 12;
		c = target ? window_client_find(target) : NULL;

		if (m == &desktop_menu && c != NULL &&
			desktop_for_client(c) == i)
			wm_draw_text(m->window, 10, row_y(m, i), 16, ROW_HEIGHT,
				"*", wm.colors[active ? WHITE : BLACK]);
		wm_draw_text(m->window, inset, row_y(m, i),
			m->width - inset -
				(m == &main_menu && target == 0 ? 12 : 24),
			ROW_HEIGHT, text,
			wm.colors[!enabled(m, i) ? SHADOW
					: active ? WHITE
						: BLACK]);
	}
}

static void
close_desktops(void)
{
	if (desktops_open)
		XUnmapWindow(wm.display, desktop_menu.window);
	desktops_open = 0;
	desktop_menu.selected = desktop_menu.pressed = -1;
}

void
owm_menu_close(void)
{
	if (!opened)
		return;

	close_desktops();
	opened = 0;
	XUngrabPointer(wm.display, CurrentTime);
	XUngrabKeyboard(wm.display, CurrentTime);
	XUnmapWindow(wm.display, main_menu.window);
	main_menu.selected = main_menu.pressed = -1;
	target = None;
}

void
owm_menu_free(void)
{
	owm_menu_close();
	if (main_menu.window)
		wm_draw_destroy_window(main_menu.window);
	if (desktop_menu.window)
		wm_draw_destroy_window(desktop_menu.window);
	main_menu.window = desktop_menu.window = None;
}

static void
prepare(struct menu *m)
{
	int i;

	m->first = 0;
	m->selected = m->pressed = m->failed = -1;
	m->height = 2 * PADDING + title_height(m);
	m->width = m == &main_menu
		? font_width(wm.font, "Action failed; try again", 23) + 28
		: 0;

	for (i = 0; i < m->count; i++) {
		int size;
		const char *label = NULL;

		m->height += row_height(m, i);
		label = m->items[i].label;
		size = font_width(wm.font, label, (int)strlen(label)) +
			(m == &desktop_menu	 ? 52
					: target ? 40
						: 28);

		if (size > m->width)
			m->width = size;
	}

	if (m->height > desktop_work_height())
		m->height = desktop_work_height();
	if (m->width > wm.width)
		m->width = wm.width;
}

static void
show(struct menu *m, int x, int y, const char *name)
{
	if (m->window == 0) {
		XSetWindowAttributes a = {0};

		a.override_redirect = True;
		a.background_pixel = wm.colors[FACE];
		a.event_mask = ExposureMask | ButtonPressMask |
			ButtonReleaseMask | PointerMotionMask | KeyPressMask;

		m->window = XCreateWindow(wm.display, wm.root, 0, 0,
			(unsigned int)m->width, (unsigned int)m->height, 0,
			CopyFromParent, InputOutput, CopyFromParent,
			CWOverrideRedirect | CWBackPixel | CWEventMask, &a);
	}
	XStoreName(wm.display, m->window, name);
	if (x + m->width > wm.width)
		x = wm.width - m->width;
	if (y + m->height > desktop_work_height())
		y = desktop_work_height() - m->height;
	m->x = x < 0 ? 0 : x;
	m->y = y < 0 ? 0 : y;
	XMoveResizeWindow(wm.display, m->window, m->x, m->y,
		(unsigned int)m->width, (unsigned int)m->height);
	XMapRaised(wm.display, m->window);
	draw(m);
}

static int
desktop_entry(int index)
{
	return target && index >= 0 && index < main_menu.count &&
		strcmp(main_menu.items[index].action, "desktops") == 0;
}

static void
open_desktops(int keyboard)
{
	struct client *c = NULL;

	if (!desktop_entry(main_menu.selected))
		return;

	if (!desktops_open) {
		int i, x;

		for (i = 0; i < wm.config.desktops; i++) {
			snprintf(desktop_items[i].label,
				sizeof(desktop_items[i].label), "Desktop %d",
				i + 1);
			snprintf(desktop_items[i].action,
				sizeof(desktop_items[i].action),
				"window_send_to_desktop %d", i + 1);
		}

		desktop_menu.items = desktop_items;
		desktop_menu.count = wm.config.desktops;
		prepare(&desktop_menu);
		x = main_menu.x + main_menu.width;

		if (x + desktop_menu.width > wm.width)
			x = main_menu.x - desktop_menu.width;
		show(&desktop_menu, x,
			main_menu.y + row_y(&main_menu, main_menu.selected) -
				PADDING,
			"owm desktop submenu");
		desktops_open = 1;
	}

	if (!keyboard)
		return;

	c = window_client_find(target);

	desktop_menu.selected = (c != NULL) ? desktop_for_client(c) : 0;
	if (desktop_menu.selected < 0)
		desktop_menu.selected = 0;
	/*
	 * Keep the current desktop visible even in a scrolling submenu.
	 */
	desktop_menu.selected--;
	choose(&desktop_menu, 1);
	draw(&desktop_menu);
}

static void
open_popup(int x, int y, Time time)
{
	int actionable, i;

	if (window_frame_drag_active() || window_move_active())
		return;

	actionable = 0;

	for (i = 0; i < main_menu.count; i++)
		actionable |= !separator(&main_menu, i);

	if (!actionable)
		return;

	prepare(&main_menu);
	show(&main_menu, x, y, target ? "owm window menu" : "owm desktop menu");
	if (XGrabPointer(wm.display, main_menu.window, False,
		ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
		GrabModeAsync, GrabModeAsync, None, wm.normal_cursor,
		time) != GrabSuccess) {
		XUnmapWindow(wm.display, main_menu.window);
		return;
	}
	if (XGrabKeyboard(wm.display, main_menu.window, False, GrabModeAsync,
		GrabModeAsync, time) != GrabSuccess) {
		XUngrabPointer(wm.display, time);
		XUnmapWindow(wm.display, main_menu.window);
		return;
	}
	opened = 1;
}

void
owm_menu_open(int x, int y, Time time)
{
	owm_menu_close();
	target = None;
	main_menu.items = wm.config.menu;
	main_menu.count = wm.config.menu_count;
	open_popup(x, y, time);
}

void
owm_menu_open_window(struct client *c, int x, int y, Time time, int keyboard)
{
	if (c == NULL || !window_client_visible(c) ||
		window_frame_drag_active() || window_move_active())
		return;

	owm_menu_close();
	window_client_focus(c, time);
	target = c->window;
	main_menu.items = window_items;
	main_menu.count = sizeof(window_items) / sizeof(*window_items);
	string_copy(window_items[1].label,
		c->max_h || c->max_v ? "Restore" : "Maximize",
		sizeof(window_items[1].label));
	string_copy(window_items[4].label,
		c->fullscreen ? "Leave fullscreen" : "Fullscreen",
		sizeof(window_items[4].label));
	string_copy(window_items[3].label,
		desktop_for_client(c) == -1 ? "Unsticky" : "Sticky",
		sizeof(window_items[3].label));
	open_popup(x, y, time);
	if (!opened || !keyboard)
		return;

	choose(&main_menu, 1);
	draw(&main_menu);
}

void
owm_menu_forget(struct client *c)
{
	if (target == c->window)
		owm_menu_close();
}

static void
activate(struct menu *m, int index, Time time)
{
	const char *action = NULL;

	if (index < 0 || index >= m->count || separator(m, index) ||
		!enabled(m, index))
		return;
	if (m == &main_menu && desktop_entry(index)) {
		open_desktops(1);
		return;
	}

	action = m->items[index].action;
	if (target) {
		struct client *c = NULL;

		c = window_client_find(target);

		/* Release grabs before entering another modal action. */
		owm_menu_close();
		if (c != NULL && window_client_visible(c)) {
			window_client_focus(c, time);
			owm_action_execute(action, 0, time);
		}

		return;
	}
	if (!owm_action_execute(action, 0, time)) {
		m->failed = index;
		draw(m);
		return;
	}
	owm_menu_close();
}

static int
contains(struct menu *m, int x, int y)
{
	return x >= m->x && x < m->x + m->width && y >= m->y &&
		y < m->y + m->height;
}

static struct menu *
menu_at(int x, int y)
{
	if (desktops_open && contains(&desktop_menu, x, y))
		return &desktop_menu;
	if (contains(&main_menu, x, y))
		return &main_menu;

	return NULL;
}

static int
handle_expose(XExposeEvent *event)
{
	struct menu *m = NULL;

	m = event->window == main_menu.window ? &main_menu
		: desktops_open && event->window == desktop_menu.window
		? &desktop_menu
		: NULL;
	if (m == NULL)
		return 0;
	if (event->count == 0)
		draw(m);

	return 1;
}

static void
handle_motion(XMotionEvent *event)
{
	struct menu *m = NULL;
	int item;

	m = menu_at(event->x_root, event->y_root);
	if (m == NULL)
		return;
	item = item_at(m, event->x_root - m->x, event->y_root - m->y);
	/* Keep the submenu open while crossing the parent's padding. */
	if (m == &main_menu && desktops_open && item < 0)
		return;
	if (m->selected != item) {
		m->selected = item;
		draw(m);
	}
	if (m == &main_menu) {
		if (desktop_entry(item))
			open_desktops(0);
		else
			close_desktops();
	}
}

static void
handle_button_press(XButtonEvent *event)
{
	struct menu *m = NULL;

	m = menu_at(event->x_root, event->y_root);
	main_menu.pressed = desktop_menu.pressed = -1;
	if (m == NULL || event->button == Button3) {
		owm_menu_close();
		return;
	}
	if (m == &main_menu)
		close_desktops();
	if (event->button == Button4 || event->button == Button5) {
		if (m->selected < 0 && event->button == Button4)
			m->selected = 0;
		choose(m, event->button == Button4 ? -1 : 1);
	} else if (event->button == Button1) {
		m->selected =
			item_at(m, event->x_root - m->x, event->y_root - m->y);
		m->pressed = m->selected;
	}
	draw(m);
}

static void
handle_button_release(XButtonEvent *event)
{
	struct menu *m = NULL;

	m = menu_at(event->x_root, event->y_root);
	if (m != NULL && event->button == Button1) {
		int index;

		index = item_at(m, event->x_root - m->x, event->y_root - m->y);
		if (index == m->pressed)
			activate(m, index, event->time);
	}
	main_menu.pressed = desktop_menu.pressed = -1;
}

static void
handle_key(XKeyEvent *event)
{
	struct menu *m = NULL;
	KeySym key;

	key = XLookupKeysym(event, 0);
	m = desktops_open ? &desktop_menu : &main_menu;
	switch (key) {
	case XK_Escape:
	case XK_Left:
		if (desktops_open)
			close_desktops();
		else if (key == XK_Escape)
			owm_menu_close();
		return;
	case XK_Right:
		if (!desktops_open)
			open_desktops(1);
		return;
	case XK_Return:
	case XK_KP_Enter:
	case XK_space:
		activate(m, m->selected, event->time);
		return;
	case XK_Down:
		choose(m, 1);
		break;
	case XK_Up:
		if (m->selected < 0)
			m->selected = 0;
		choose(m, -1);
		break;
	case XK_Home:
		m->selected = -1;
		choose(m, 1);
		break;
	case XK_End:
		m->selected = 0;
		choose(m, -1);
		break;
	default:
		break;
	}
	draw(m);
}

int
owm_menu_event(XEvent *e)
{
	if (!opened)
		return 0;
	if (target) {
		struct client *c = NULL;

		c = window_client_find(target);
		if (c == NULL || !window_client_visible(c)) {
			owm_menu_close();
			return 0;
		}
	}

	switch (e->type) {
	case Expose:
		return handle_expose(&e->xexpose);

	case MotionNotify:
		handle_motion(&e->xmotion);
		return 1;

	case ButtonPress:
		handle_button_press(&e->xbutton);
		return 1;

	case ButtonRelease:
		handle_button_release(&e->xbutton);
		return 1;

	case KeyPress:
		handle_key(&e->xkey);
		return 1;

	default:
		return 0;
	}
}
