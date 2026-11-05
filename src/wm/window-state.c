#include <string.h>

#include "wm.h"

static int	clamp(int, int, int);
static void	fit_client(struct client *, struct geometry *);
static void	read_initial_state(struct client *);
static void	restore_placement(struct client *);

static int
clamp(int v, int low, int high)
{
	if (high < low)
		high = low;

	return v < low ? low : v > high ? high : v;
}

int
window_client_minimized(const struct client *c)
{
	int depth;

	for (depth = 0; c != NULL && depth < MAX_TRANSIENT_DEPTH; depth++) {
		if (c->minimized)
			return 1;

		c = window_client_find(c->transient);
	}

	return 0;
}

int
window_client_visible(const struct client *c)
{
	return desktop_contains(c) && !window_client_minimized(c);
}

void
window_client_constrain_size(struct client *c, int *width, int *height)
{
	XSizeHints *h = NULL;
	int baseh = 0, basew = 0, inch = 1, incw = 1;
	int height_value, w;
	int maxh = WM_GEOMETRY_LIMIT, maxw = WM_GEOMETRY_LIMIT, minh = 1,
	minw = 1;

	h = &c->hints;

	if (h->flags & PMinSize) {
		minw = clamp(h->min_width, 1, maxw);
		minh = clamp(h->min_height, 1, maxh);
	}

	if (h->flags & PMaxSize) {
		maxw = clamp(h->max_width, minw, maxw);
		maxh = clamp(h->max_height, minh, maxh);
	}

	if (h->flags & PBaseSize) {
		basew = clamp(h->base_width, 0, maxw);
		baseh = clamp(h->base_height, 0, maxh);
	} else if (h->flags & PMinSize) {
		basew = minw;
		baseh = minh;
	}

	if (h->flags & PResizeInc) {
		incw = clamp(h->width_inc, 1, maxw);
		inch = clamp(h->height_inc, 1, maxh);
	}

	w = clamp(*width, minw, maxw);
	height_value = clamp(*height, minh, maxh);

	if ((h->flags & PAspect) && h->min_aspect.x > 0 &&
		h->min_aspect.y > 0 && h->max_aspect.x > 0 &&
		h->max_aspect.y > 0) {
		double ratio;

		ratio = (double)w / height_value;

		if (ratio < (double)h->min_aspect.x / h->min_aspect.y)
			height_value = clamp((int)((double)w * h->min_aspect.y /
						h->min_aspect.x),
				minh, maxh);
		else if (ratio > (double)h->max_aspect.x / h->max_aspect.y)
			w = clamp((int)((double)height_value * h->max_aspect.x /
					h->max_aspect.y),
				minw, maxw);
	}

	if (w >= basew)
		w = basew + (w - basew) / incw * incw;

	if (height_value >= baseh)
		height_value = baseh + (height_value - baseh) / inch * inch;
	*width = clamp(w, minw, maxw);
	*height = clamp(height_value, minh, maxh);
}

/* Mouse and keyboard moves keep the same amount of titlebar reachable. */
void
window_client_constrain_move(struct geometry *geometry)
{
	if (geometry->x < 48 - geometry->w)
		geometry->x = 48 - geometry->w;

	if (geometry->x > wm.width - 48)
		geometry->x = wm.width - 48;

	if (geometry->y < 0)
		geometry->y = 0;

	if (geometry->y > desktop_work_height() - window_frame_top())
		geometry->y = desktop_work_height() - window_frame_top();
}

static void
fit_client(struct client *c, struct geometry *g)
{
	g->w = clamp(g->w, 1, wm.width - 2 * BORDER);
	g->h = clamp(
		g->h, 1, desktop_work_height() - window_frame_top() - BORDER);
	window_client_constrain_size(c, &g->w, &g->h);
	g->x = clamp(g->x, 0, wm.width - g->w - 2 * BORDER);
	g->y = clamp(g->y, 0,
		desktop_work_height() - g->h - window_frame_top() - BORDER);
}

void
window_client_configure(struct client *c, struct geometry g, int constrain)
{
	int moved, redecorated, resized;
	unsigned int decoration;

	if (constrain)
		window_client_constrain_size(c, &g.w, &g.h);
	g.w = clamp(g.w, 1, WM_GEOMETRY_LIMIT);
	g.h = clamp(g.h, 1, WM_GEOMETRY_LIMIT);
	decoration = c->fullscreen | (c->max_h << 1) | (c->max_v << 2);

	resized = !c->configured || g.w != c->applied.w ||
		g.h != c->applied.h ||
		((decoration ^ c->applied_decoration) & 1);
	moved = !c->configured || g.x != c->applied.x || g.y != c->applied.y;
	redecorated = !c->configured || decoration != c->applied_decoration;

	c->g = g;
	/* Keep changes on each unmaximized axis available for restoration. */
	if (!c->fullscreen) {
		if (!c->max_h) {
			c->restore.x = g.x;
			c->restore.w = g.w;
		}
		if (!c->max_v) {
			c->restore.y = g.y;
			c->restore.h = g.h;
		}
	}

	if (resized) {
		XMoveResizeWindow(wm.display, c->frame, g.x, g.y,
			window_frame_width(c), window_frame_height(c));
		XMoveResizeWindow(wm.display, c->window,
			c->fullscreen ? 0 : BORDER,
			c->fullscreen ? 0 : window_frame_top(), g.w, g.h);
	} else if (moved) {
		XMoveWindow(wm.display, c->frame, g.x, g.y);
	}

	if (resized || moved || redecorated) {
		struct geometry saved;
		unsigned long values[4] = {0};

		saved = c->fullscreen || c->max_h || c->max_v ? c->restore : g;
		values[0] = (unsigned long)saved.x;
		values[1] = (unsigned long)saved.y;
		values[2] = (unsigned long)saved.w;
		values[3] = (unsigned long)saved.h;

		wm_property_set_cardinals(
			c->window, wm.atoms[A_RESTORE_GEOMETRY], values, 4);
	}

	/*
	 * Even unchanged client ConfigureRequests require a synthetic
	 * notification.
	 */
	window_client_send_configure_notify(c);

	if (resized || redecorated)
		window_frame_draw(c);
	c->applied = g;
	c->applied_decoration = decoration;
	c->configured = 1;
}

void
window_client_sync_visibility(void)
{
	struct client *p = NULL;

	for (p = wm.clients; p != NULL; p = p->next) {
		unsigned long desktop;

		p->desktop = desktop_for_client(p);

		if (desktop_contains(p))
			p->desktop_fullscreen = 0;

		desktop = (unsigned long)p->desktop;

		wm_property_set_cardinals(
			p->window, wm.atoms[A_WM_DESKTOP], &desktop, 1);

		if (!window_client_visible(p))
			XUnmapWindow(wm.display, p->frame);
		else {
			XMapWindow(wm.display, p->window);
			XMapWindow(wm.display, p->frame);
		}

		window_client_publish_state(p);
	}
}

static void
read_initial_state(struct client *c)
{
	XWMHints *hint = NULL;
	unsigned long n;
	unsigned char *desktop_data = NULL, *state = NULL;

	c->fullscreen = wm_property_has_atom(
		c->window, wm.atoms[A_NET_STATE], wm.atoms[A_FULLSCREEN]);
	c->max_h = wm_property_has_atom(
		c->window, wm.atoms[A_NET_STATE], wm.atoms[A_MAX_HORZ]);
	c->max_v = wm_property_has_atom(
		c->window, wm.atoms[A_NET_STATE], wm.atoms[A_MAX_VERT]);

	desktop_data = wm_property_get(
		c->window, wm.atoms[A_WM_DESKTOP], XA_CARDINAL, 32, &n);

	if (desktop_data != NULL && n) {
		unsigned long desktop;

		desktop = ((unsigned long *)desktop_data)[0] & 0xffffffffUL;

		c->desktop = desktop == 0xffffffffUL ? -1
			: desktop < (unsigned long)wm.config.desktops
			? (int)desktop
			: wm.config.desktops - 1;
	}

	if (desktop_data != NULL)
		XFree(desktop_data);

	state = wm_property_get(
		c->window, wm.atoms[A_STATE], wm.atoms[A_STATE], 32, &n);

	c->minimized = state != NULL && n && ((long *)state)[0] == IconicState;

	if (state != NULL)
		XFree(state);

	hint = XGetWMHints(wm.display, c->window);

	if (hint != NULL && (hint->flags & StateHint) &&
		hint->initial_state == IconicState && !wm.scanning)
		c->minimized = 1;

	if (hint != NULL)
		XFree(hint);

	/*
	 * Dialog iconification follows its parent rather than an independent
	 * task.
	 */
	if (c->transient)
		c->minimized = 0;
}

static void
restore_placement(struct client *c)
{
	unsigned long n;
	int had_geometry;
	unsigned char *saved = NULL;

	saved = wm_property_get(
		c->window, wm.atoms[A_RESTORE_GEOMETRY], XA_CARDINAL, 32, &n);
	had_geometry = saved != NULL && n == 4;

	if (had_geometry) {
		unsigned long *v = NULL;

		v = (unsigned long *)saved;

		c->g.x = (int)v[0];
		c->g.y = (int)v[1];
		c->g.w = (int)v[2];
		c->g.h = (int)v[3];
	} else if (!wm.scanning &&
		(c->hints.flags & (USPosition | PPosition)) == 0) {
		struct client *parent = NULL;

		parent = window_client_find(c->transient);

		if (parent != NULL) {
			c->g.x = parent->g.x + (parent->g.w - c->g.w) / 2;
			c->g.y = parent->g.y + (parent->g.h - c->g.h) / 2;
		} else
			c->g.x = c->g.y = 24 + (c->order % 10) * 24;
	}

	if (saved != NULL)
		XFree(saved);
	saved = wm_property_get(
		c->window, wm.atoms[A_ORDER], XA_CARDINAL, 32, &n);

	if (saved != NULL && n && ((unsigned long *)saved)[0] < INT_MAX - 1)
		c->order = (int)((unsigned long *)saved)[0];

	if (saved != NULL)
		XFree(saved);
	c->restore = c->g;
	if (!owm_session_match(c) && !had_geometry && !wm.scanning)
		window_placement_get(c, &c->g.x, &c->g.y);

	if (c->order >= wm.next_order)
		wm.next_order = c->order + 1;
	fit_client(c, &c->g);
	c->restore = c->g;
}

void
window_client_minimize(struct client *c, int minimize)
{
	if (minimize) {
		struct client *ancestor = NULL;

		/*
		 * A dialog has no separate task button: iconify its owning
		 * group.
		 */

		ancestor = window_client_ancestor(c);
		if (ancestor != NULL)
			c = ancestor;
	}

	if (c->minimized == minimize)
		return;

	c->minimized = minimize;
	window_client_sync_visibility();

	if (wm.focused != NULL && !window_client_visible(wm.focused))
		window_client_focus_topmost();
	taskbar_draw();
}

void
window_client_maximize(struct client *c, int horizontal, int vertical)
{
	struct geometry g;
	int h, w;

	if ((horizontal || vertical) && window_frame_resizing(c))
		window_frame_cancel_drag(c);

	if (!c->fullscreen && !c->max_h && !c->max_v &&
		(horizontal || vertical))
		c->restore = c->g;
	c->max_h = horizontal;
	c->max_v = vertical;

	if (c->fullscreen) {
		window_client_publish_state(c);
		return;
	}

	g = c->restore;
	w = wm.width - 2 * BORDER;
	h = desktop_work_height() - window_frame_top() - BORDER;

	window_client_constrain_size(c, &w, &h);

	/*
	 * Work area takes precedence over impossible minimum size hints when
	 * maximized.
	 */
	if (horizontal) {
		g.x = 0;
		g.w = clamp(w, 1, wm.width - 2 * BORDER);
	}

	if (vertical) {
		g.y = 0;
		g.h = clamp(h, 1,
			desktop_work_height() - window_frame_top() - BORDER);
	}

	window_client_configure(c, g, 0);
	window_client_publish_state(c);
	taskbar_draw();
}

void
window_client_fullscreen(struct client *c, int fullscreen)
{
	if (c->fullscreen == fullscreen)
		return;

	window_frame_cancel_drag(c);

	if (fullscreen && !c->max_h && !c->max_v)
		c->restore = c->g;
	c->fullscreen = fullscreen;

	wm_protocol_frame_extents(c->window, c->fullscreen);

	if (fullscreen) {
		struct geometry screen = {0};

		screen.x = 0;
		screen.y = 0;
		screen.w = wm.width;
		screen.h = wm.height;

		window_client_configure(c, screen, 0);
		window_client_publish_state(c);
	} else if (c->max_h || c->max_v) {
		window_client_maximize(c, c->max_h, c->max_v);
	} else {
		/*
		 * SDL may restore the desktop video mode after leaving
		 * fullscreen. Do not clamp the saved window against the
		 * temporary game resolution.
		 */
		window_client_configure(c, c->restore, 0);
		window_client_publish_state(c);
	}

	window_client_restack();
}

void
window_client_handle_configure_request(XConfigureRequestEvent *e)
{
	struct client *c = NULL, *sibling = NULL;
	struct geometry g;
	XWindowChanges wc = {0};
	unsigned int mask;

	c = window_client_find(e->window);

	if (c == NULL) {
		wc.x = e->x;
		wc.y = e->y;
		wc.width = e->width;
		wc.height = e->height;
		wc.border_width = e->border_width;
		wc.sibling = e->above;
		wc.stack_mode = e->detail;

		XConfigureWindow(wm.display, e->window,
			(unsigned int)e->value_mask, &wc);

		return;
	}

	if (c->fullscreen) {
		/*
		 * Clients may resize themselves during a video mode transition.
		 * The WM owns fullscreen geometry. Acknowledge without losing
		 * the saved size.
		 */
		window_client_send_configure_notify(c);
		return;
	}

	g = c->g;

	if (!c->max_h) {
		if (e->value_mask & CWX)
			g.x = e->x;

		if (e->value_mask & CWWidth)
			g.w = e->width;
	}

	if (!c->max_v) {
		if (e->value_mask & CWY)
			g.y = e->y;

		if (e->value_mask & CWHeight)
			g.h = e->height;
	}

	window_client_configure(c, g, !(c->max_h || c->max_v));

	if ((e->value_mask & CWStackMode) == 0)
		return;

	wc.stack_mode = e->detail;
	mask = CWStackMode;

	sibling = window_client_find(e->above);

	if ((e->value_mask & CWSibling) && sibling != NULL && sibling != c) {
		wc.sibling = sibling->frame;
		mask |= CWSibling;
	}

	XConfigureWindow(wm.display, c->frame, mask, &wc);
	window_client_restack();
}

void
window_state_snapshot(const struct client *c, struct window_state *state)
{
	state->g = c->g;
	state->restore = c->restore;
	state->minimized = c->minimized;
	state->fullscreen = c->fullscreen;
	state->max_h = c->max_h;
	state->max_v = c->max_v;
}

/* Seed placement before the frame exists. Activation applies it to X11. */
void
window_state_seed(struct client *c, const struct window_state *state)
{
	c->g = state->g;
	c->restore = state->restore;
	c->minimized = state->minimized;
	c->fullscreen = state->fullscreen;
	c->max_h = state->max_h;
	c->max_v = state->max_v;
}

void
window_state_initialize(struct client *c, const XWindowAttributes *a,
	struct window_state *initial)
{
	c->g.x = a->x;
	c->g.y = a->y;
	c->g.w = a->width;
	c->g.h = a->height;
	read_initial_state(c);
	restore_placement(c);
	window_state_snapshot(c, initial);
	c->fullscreen = c->max_h = c->max_v = 0;
}

void
window_state_activate(struct client *c, const struct window_state *initial)
{
	if (initial->max_h || initial->max_v)
		window_client_maximize(c, initial->max_h, initial->max_v);
	if (initial->fullscreen)
		window_client_fullscreen(c, 1);
}

void
window_state_restore_move(struct client *c, const struct window_state *saved)
{
	c->max_h = saved->max_h;
	c->max_v = saved->max_v;
	c->restore = saved->restore;
	window_client_configure(c, saved->g, 0);
	window_client_publish_state(c);
	window_client_restack();
}

void
window_state_request_fullscreen(struct client *c, int fullscreen)
{
	if (fullscreen == c->fullscreen)
		return;
	c->desktop_fullscreen = c->fullscreen && !desktop_contains(c);
	window_client_fullscreen(c, fullscreen);
}

void
window_state_request_iconify(struct client *c)
{
	/* Keep SDL games available after iconification on another desktop. */
	if (desktop_contains(c) || (!c->fullscreen && !c->desktop_fullscreen))
		window_client_minimize(c, 1);
	else if (c->desktop_fullscreen) {
		c->desktop_fullscreen = 0;
		window_client_fullscreen(c, 1);
	}
}
