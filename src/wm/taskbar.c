#include "clock.h"
#include "tray-widgets.h"
#include "wm.h"

enum {
	BUTTON_MIN_WIDTH = 72,
	BUTTON_MAX_WIDTH = 180,
	BUTTON_GAP = 3,
	PAGER_CELL_WIDTH = 24,
	PAGER_BUTTON_WIDTH = 22,
	OVERFLOW_CELL_WIDTH = 20
};

static int first_button;
static int last_desktop = -1;

struct layout {
	int	desktops[MAX_DESKTOPS];
	int	occupied[MAX_DESKTOPS];
	int	desktop_count;
	int	count;
	int	shown;
	int	start;
	int	width;
	int	right;
	int	overflow;
};

enum button_redraw {
	REDRAW_ALL,
	REDRAW_CLIENT,
	REDRAW_FOCUS
};

static int		pager_width(const struct layout *);
static void		layout_desktops(struct layout *);
static int		pager_start(void);
static int		tray_start(void);
static int		task_entry(const struct client *);
static struct layout	layout(void);
static int		active_for(struct client *, struct client *);
static int		active(struct client *);
static void		draw_button(struct client *, int, int);
static void		draw_buttons(const struct layout *, enum button_redraw,
	struct client *);
static void		redraw_buttons(enum button_redraw, struct client *);
static void		draw_overflow(const struct layout *, int);
static void		draw_tray_border(int);
static void		draw_pager(const struct layout *, int);

/* Keep the original proportions at 28px and grow controls with the bar. */
int
taskbar_pixels(int pixels)
{
	int height;

	height = wm.config.taskbar_height;

	return pixels *
		(height > TASKBAR_BASE_HEIGHT ? height : TASKBAR_BASE_HEIGHT) /
		TASKBAR_BASE_HEIGHT;
}

static int
pager_width(const struct layout *l)
{
	return l->desktop_count
		? l->desktop_count * taskbar_pixels(PAGER_CELL_WIDTH) + 4
		: 0;
}

static void
layout_desktops(struct layout *l)
{
	struct client *c = NULL;
	int desktop, sticky = 0;

	for (c = wm.clients; c != NULL; c = c->next) {
		if (!window_client_is_task(c))
			continue;
		desktop = desktop_for_client(c);
		if (desktop == -1)
			sticky = 1;
		else if (desktop >= 0 && desktop < wm.config.desktops)
			l->occupied[desktop] = 1;
	}
	for (desktop = 0; desktop < wm.config.desktops; desktop++) {
		l->occupied[desktop] |= sticky;
		if (!wm.config.taskbar_hide_empty_desktops ||
			l->occupied[desktop] || desktop == wm.current_desktop)
			l->desktops[l->desktop_count++] = desktop;
	}
	if (wm.config.taskbar_hide_single_desktop && l->desktop_count == 1)
		l->desktop_count = 0;
}

static int
pager_start(void)
{
	return 4;
}

static int
tray_start(void)
{
	return tray_widgets_left() - 4;
}

static int
task_entry(const struct client *c)
{
	return window_client_is_task(c) && desktop_contains(c);
}

static struct layout
layout(void)
{
	struct client *c = NULL;
	struct layout l = {0};
	int available;

	if (last_desktop != wm.current_desktop) {
		first_button = 0;
		last_desktop = wm.current_desktop;
	}

	for (c = wm.clients; c != NULL; c = c->next)
		if (task_entry(c))
			l.count++;

	l.right = tray_start() - 4;
	layout_desktops(&l);
	l.start = pager_start() + pager_width(&l);

	available = l.right - l.start;

	l.overflow = l.count > available / taskbar_pixels(BUTTON_MIN_WIDTH);

	if (l.overflow) {
		l.start += taskbar_pixels(OVERFLOW_CELL_WIDTH);
		available -= taskbar_pixels(2 * OVERFLOW_CELL_WIDTH);
	}

	l.shown = l.overflow ? available / taskbar_pixels(BUTTON_MIN_WIDTH)
			: l.count;

	if (l.shown < 1)
		l.shown = 1;

	if (l.shown > l.count)
		l.shown = l.count;

	if (first_button > l.count - l.shown)
		first_button = l.count - l.shown;

	if (first_button < 0 || !l.overflow)
		first_button = 0;
	l.width = l.shown ? available / l.shown : 0;

	if (l.width > taskbar_pixels(BUTTON_MAX_WIDTH))
		l.width = taskbar_pixels(BUTTON_MAX_WIDTH);

	return l;
}

static int
active_for(struct client *c, struct client *p)
{
	int i;

	for (i = 0; p != NULL && i < MAX_TRANSIENT_DEPTH; i++) {
		if (p == c)
			return 1;

		p = window_client_find(p->transient);
	}

	return 0;
}

static int
active(struct client *c)
{
	return active_for(c, wm.focused);
}

static void
draw_button(struct client *c, int x, int width)
{
	int height, pressed;

	height = wm.config.taskbar_height;
	pressed = active(c) && !c->minimized;

	wm_draw_surface(wm.taskbar, x, 4, width - BUTTON_GAP, height - 7,
		wm.colors[pressed ? LIGHT : FACE], pressed);

	if (width >= taskbar_pixels(28))
		taskbar_icon_draw(c, wm.taskbar,
			x + taskbar_pixels(5) + pressed,
			(height - taskbar_pixels(TASKBAR_ICON_SIZE)) / 2 +
				pressed,
			pressed);
	wm_draw_text(wm.taskbar, x + taskbar_pixels(25) + pressed, 5 + pressed,
		width - taskbar_pixels(35), height - 9, c->title,
		wm.colors[BLACK]);
}

/* Target is the changed client or previous focus, according to the mode. */
static void
draw_buttons(
	const struct layout *l, enum button_redraw mode, struct client *target)
{
	struct client *c = NULL;
	int i, visible;

	i = 0;
	visible = 0;

	for (c = wm.clients; c != NULL; c = c->next) {
		int x;

		if (!task_entry(c))
			continue;

		if (i++ < first_button)
			continue;

		if (visible >= l->shown)
			break;

		x = l->start + visible++ * l->width;

		if (mode == REDRAW_CLIENT && c != target)
			continue;
		if (mode == REDRAW_FOCUS && active_for(c, target) == active(c))
			continue;
		draw_button(c, x, l->width);
	}
}

/* Title/icon and focus changes cannot change the button layout. */
static void
redraw_buttons(enum button_redraw mode, struct client *target)
{
	struct layout l;

	if (wm.taskbar == 0)
		return;
	l = layout();
	draw_buttons(&l, mode, target);
}

void
taskbar_draw_client(struct client *c)
{
	redraw_buttons(REDRAW_CLIENT, c);
}

void
taskbar_focus_changed(struct client *previous_focus)
{
	redraw_buttons(REDRAW_FOCUS, previous_focus);
}

void
taskbar_resize(void)
{
	unsigned long strut[12] = {0};

	XMoveResizeWindow(wm.display, wm.taskbar, 0, desktop_work_height(),
		(unsigned int)wm.width, (unsigned int)wm.config.taskbar_height);

	strut[3] = (unsigned long)wm.config.taskbar_height;
	strut[10] = 0;
	strut[11] = (unsigned long)(wm.width - 1);
	wm_property_set_cardinals(wm.taskbar, wm.atoms[A_STRUT], strut, 4);
	wm_property_set_cardinals(
		wm.taskbar, wm.atoms[A_STRUT_PARTIAL], strut, 12);
	taskbar_draw();
}

void
taskbar_init(void)
{
	XSetWindowAttributes a = {0};
	Atom dock;

	a.override_redirect = True;
	a.background_pixel = wm.colors[FACE];
	a.event_mask = ExposureMask | ButtonPressMask | PointerMotionMask |
		EnterWindowMask | LeaveWindowMask;

	wm.taskbar = XCreateWindow(wm.display, wm.root, 0,
		desktop_work_height(), (unsigned int)wm.width,
		(unsigned int)wm.config.taskbar_height, 0, CopyFromParent,
		InputOutput, CopyFromParent,
		CWOverrideRedirect | CWBackPixel | CWEventMask, &a);
	XStoreName(wm.display, wm.taskbar, "owm taskbar");

	dock = wm.atoms[A_TYPE_DOCK];

	XChangeProperty(wm.display, wm.taskbar, wm.atoms[A_TYPE], XA_ATOM, 32,
		PropModeReplace, (unsigned char *)&dock, 1);
	taskbar_icons_init();
	tray_widgets_init();
	tray_widgets_update(monotonic_time());
	taskbar_resize();
	XMapRaised(wm.display, wm.taskbar);
}

void
taskbar_free(void)
{
	taskbar_tooltip_free();
	tray_widgets_free();
	taskbar_icons_free();
	if (wm.taskbar)
		wm_draw_destroy_window(wm.taskbar);
	wm.taskbar = None;
	first_button = 0;
	last_desktop = -1;
}

static void
draw_overflow(const struct layout *layout, int height)
{
	int left;

	if (!layout->overflow)
		return;

	left = pager_start() + pager_width(layout);

	wm_draw_box(wm.taskbar, left, 4, taskbar_pixels(18), height - 7, 0);
	wm_draw_text(wm.taskbar, left + taskbar_pixels(6), 5, taskbar_pixels(8),
		height - 9, "<", wm.colors[BLACK]);
	wm_draw_box(wm.taskbar,
		layout->right - taskbar_pixels(OVERFLOW_CELL_WIDTH), 4,
		taskbar_pixels(18), height - 7, 0);
	wm_draw_text(wm.taskbar, layout->right - taskbar_pixels(14), 5,
		taskbar_pixels(8), height - 9, ">", wm.colors[BLACK]);
}

static void
draw_tray_border(int height)
{
	int tray_left, tray_right;

	tray_left = tray_start();
	tray_right = wm.width - 5;

	XSetForeground(wm.display, wm.gc, wm.colors[SHADOW]);
	XDrawLine(wm.display, wm.taskbar, wm.gc, tray_left, 4, tray_right, 4);
	XDrawLine(wm.display, wm.taskbar, wm.gc, tray_left, 4, tray_left,
		height - 4);
	XSetForeground(wm.display, wm.gc, wm.colors[WHITE]);
	XDrawLine(wm.display, wm.taskbar, wm.gc, tray_left, height - 4,
		tray_right, height - 4);
	XDrawLine(wm.display, wm.taskbar, wm.gc, tray_right, 4, tray_right,
		height - 4);
}

static void
draw_pager(const struct layout *l, int height)
{
	int i;

	for (i = 0; i < l->desktop_count; i++) {
		int desktop, pressed, width, x;
		char label[16];

		desktop = l->desktops[i];
		x = pager_start() + i * taskbar_pixels(PAGER_CELL_WIDTH);
		pressed = desktop == wm.current_desktop;

		snprintf(label, sizeof(label), "%d", desktop + 1);

		wm_draw_surface(wm.taskbar, x, 4,
			taskbar_pixels(PAGER_BUTTON_WIDTH), height - 7,
			wm.colors[pressed ? LIGHT : FACE], pressed);

		width = font_width(wm.font, label, desktop < 9 ? 1 : 2);

		wm_draw_text(wm.taskbar,
			x + (taskbar_pixels(PAGER_BUTTON_WIDTH) - width) / 2, 5,
			width, height - 9, label,
			wm.colors[l->occupied[desktop] ? BLACK : SHADOW]);
	}
}

void
taskbar_draw_tray(void)
{
	int left, height;

	if (wm.taskbar == 0)
		return;
	left = tray_start();
	height = wm.config.taskbar_height;
	XSetForeground(wm.display, wm.gc, wm.colors[FACE]);
	XFillRectangle(wm.display, wm.taskbar, wm.gc, left, 4,
		wm.width - 4 - left, height - 7);
	draw_tray_border(height);
	tray_widgets_draw();
}

void
taskbar_draw(void)
{
	struct layout l;
	int height;

	if (wm.taskbar == 0)
		return;

	height = wm.config.taskbar_height;

	wm_draw_box(wm.taskbar, 0, 0, wm.width, height, 0);

	l = layout();
	draw_buttons(&l, REDRAW_ALL, NULL);

	draw_overflow(&l, height);
	draw_tray_border(height);
	draw_pager(&l, height);

	tray_widgets_draw();
}

struct client *
taskbar_client_at(int x, int y)
{
	struct client *c = NULL;
	struct layout l;
	int i, target;

	l = layout();
	if (y < 4 || y >= wm.config.taskbar_height - 3 || l.width <= 0 ||
		x < l.start || x >= l.start + l.shown * l.width ||
		(x - l.start) % l.width >= l.width - BUTTON_GAP)
		return NULL;

	target = first_button + (x - l.start) / l.width;
	i = 0;
	for (c = wm.clients; c != NULL; c = c->next) {
		if (!task_entry(c))
			continue;
		if (i++ == target)
			return c;
	}

	return NULL;
}

void
taskbar_press(XButtonEvent *e)
{
	struct client *c = NULL;
	struct layout l;
	int overflow_x;

	l = layout();

	if (l.desktop_count && e->x >= pager_start() &&
		e->x < pager_start() +
				l.desktop_count *
					taskbar_pixels(PAGER_CELL_WIDTH)) {
		int desktop;

		if (e->button == Button4 || e->button == Button5) {
			desktop_switch(desktop_offset(wm.current_desktop,
					e->button == Button4 ? -1 : 1),
				e->time);
			return;
		}

		if (e->button != Button1 || e->y < 4 ||
			e->y >= wm.config.taskbar_height - 3)
			return;

		desktop = l.desktops[(e->x - pager_start()) /
			taskbar_pixels(PAGER_CELL_WIDTH)];

		if (e->state & ShiftMask)
			desktop_send(wm.focused, desktop, e->time);
		else
			desktop_switch(desktop, e->time);

		return;
	}

	if (l.overflow && (e->button == Button4 || e->button == Button5)) {
		first_button += e->button == Button4 ? -l.shown : l.shown;
		taskbar_draw();
		return;
	}

	if (e->button != Button1 || e->y < 4 ||
		e->y >= wm.config.taskbar_height - 3)
		return;

	overflow_x = l.right - taskbar_pixels(OVERFLOW_CELL_WIDTH);
	if (l.overflow &&
		((e->x >= pager_start() + pager_width(&l) && e->x < l.start) ||
			(e->x >= overflow_x && e->x < l.right))) {
		first_button += e->x < l.start ? -l.shown : l.shown;
		taskbar_draw();
		return;
	}

	c = taskbar_client_at(e->x, e->y);
	if (c == NULL)
		return;

	if (active(c) && !c->minimized) {
		window_client_minimize(c, 1);
		return;
	}

	window_client_focus(c, e->time);
}
