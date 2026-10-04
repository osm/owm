#include <stdlib.h>

#include "memory.h"
#include "wm.h"

static void	sort_clients(void);
static void	attach_frame(struct client *, int);
static void	detach_frame(struct client *, int);
static void	free_client(struct client *);

struct client *
window_client_find(Window window)
{
	struct client *c = NULL;

	if (window == None)
		return NULL;

	for (c = wm.clients; c != NULL; c = c->next)
		if (c->window == window || c->frame == window)
			return c;

	return NULL;
}

struct client *
window_client_ancestor(const struct client *c)
{
	struct client *ancestor = NULL;
	int depth;

	for (depth = 0; c != NULL && depth < MAX_TRANSIENT_DEPTH; depth++) {
		struct client *parent = NULL;

		parent = window_client_find(c->transient);
		if (parent == NULL || parent == c)
			break;
		ancestor = parent;
		c = parent;
	}

	return ancestor;
}

int
window_client_is_task(const struct client *c)
{
	return !c->skip_taskbar &&
		(c->transient == 0 || window_client_find(c->transient) == NULL);
}

static void
sort_clients(void)
{
	struct client *sorted = NULL;

	while (wm.clients != NULL) {
		struct client *c = NULL;
		struct client **p = NULL;

		c = wm.clients;

		wm.clients = c->next;

		p = &sorted;

		while (*p != NULL && (*p)->order <= c->order)
			p = &(*p)->next;
		c->next = *p;
		*p = c;
	}

	wm.clients = sorted;
	window_client_publish_list();
}

static void
attach_frame(struct client *c, int map_state)
{
	unsigned long order;

	c->frame = XCreateSimpleWindow(wm.display, wm.root, c->g.x, c->g.y,
		window_frame_width(c), window_frame_height(c), 0,
		wm.colors[BLACK], wm.colors[FACE]);
	XSelectInput(wm.display, c->frame,
		EnterWindowMask | ExposureMask | ButtonPressMask |
			ButtonReleaseMask | PointerMotionMask |
			LeaveWindowMask | SubstructureRedirectMask |
			SubstructureNotifyMask);
	XSelectInput(wm.display, c->window,
		PropertyChangeMask | StructureNotifyMask | FocusChangeMask |
			EnterWindowMask);
	XAddToSaveSet(wm.display, c->window);
	XSetWindowBorderWidth(wm.display, c->window, 0);

	if (map_state != IsUnmapped)
		c->ignore_unmap++;
	XReparentWindow(
		wm.display, c->window, c->frame, BORDER, window_frame_top());
	XGrabButton(wm.display, AnyButton, AnyModifier, c->window, False,
		ButtonPressMask, GrabModeSync, GrabModeAsync, None, None);
	c->next = wm.clients;
	wm.clients = c;
	focus_cycle_add(c);

	order = (unsigned long)c->order;

	wm_property_set_cardinals(c->window, wm.atoms[A_ORDER], &order, 1);

	wm_protocol_frame_extents(c->window, c->fullscreen);
	sort_clients();
	window_client_configure(c, c->g, 1);
}

/* Caller holds the server grab. Keep the frame until detachment is complete. */
static void
detach_frame(struct client *c, int unmap)
{
	XSelectInput(wm.display, c->window, NoEventMask);
	XUngrabButton(wm.display, AnyButton, AnyModifier, c->window);
	if (unmap)
		XUnmapWindow(wm.display, c->window);
	XReparentWindow(wm.display, c->window, wm.root,
		c->g.x + (c->fullscreen ? 0 : BORDER),
		c->g.y + (c->fullscreen ? 0 : window_frame_top()));
	XSetWindowBorderWidth(wm.display, c->window, c->original_border);
	XRemoveFromSaveSet(wm.display, c->window);
}

/* Release client resources. Callers handle the frame and bookkeeping. */
static void
free_client(struct client *c)
{
	taskbar_icon_free(c);
	free(c->title);
	free(c->instance);
	free(c->class_name);
	free(c->command);
	free(c);
}

void
window_client_manage(Window window)
{
	struct client *c = NULL;
	XWindowAttributes a;
	struct window_state initial;

	if (window_client_find(window) != NULL || window == wm.taskbar ||
		window == wm.support ||
		XGetWindowAttributes(wm.display, window, &a) == 0 ||
		a.override_redirect || a.class == InputOnly)
		return;

	if (wm_property_has_atom(
		window, wm.atoms[A_TYPE], wm.atoms[A_TYPE_DOCK]) ||
		wm_property_has_atom(
			window, wm.atoms[A_TYPE], wm.atoms[A_TYPE_DESKTOP])) {
		XMapWindow(wm.display, window);
		return;
	}

	c = allocate(sizeof(*c));

	c->window = window;
	c->original_border = a.border_width;
	c->order = wm.next_order++;
	c->desktop = wm.current_desktop;
	window_client_refresh(c);
	taskbar_icon_update(c);
	window_state_initialize(c, &a, &initial);
	attach_frame(c, a.map_state);
	window_state_activate(c, &initial);
	XMapWindow(wm.display, c->window);
	window_client_sync_visibility();

	if (window_client_visible(c)) {
		XMapWindow(wm.display, c->frame);

		if (!wm.scanning)
			window_client_focus(c, CurrentTime);
	}

	taskbar_draw();
}

void
window_client_close(struct client *c, Time time)
{
	if (c->delete_window) {
		wm_protocol_send(c, wm.atoms[A_DELETE], time);
		return;
	}

	XKillClient(wm.display, c->window);
}

void
window_client_unmanage(struct client *c, int destroyed)
{
	struct client **p = NULL;
	int focused, i;

	focus_cycle_forget(c);
	window_move_forget(c);
	owm_menu_forget(c);

	focused = wm.focused == c;

	window_frame_forget(c);

	for (i = 0; i < MAX_DESKTOPS; i++)
		if (wm.desktop_focus[i] == c->window)
			wm.desktop_focus[i] = None;

	if (focused)
		wm.focused = NULL;

	p = &wm.clients;

	while (*p != NULL && *p != c)
		p = &(*p)->next;

	if (*p != NULL)
		*p = c->next;

	if (!destroyed) {
		XGrabServer(wm.display);
		detach_frame(c, 0);
		XDeleteProperty(wm.display, c->window, wm.atoms[A_STATE]);
		XDeleteProperty(
			wm.display, c->window, wm.atoms[A_FRAME_EXTENTS]);
		XDeleteProperty(
			wm.display, c->window, wm.atoms[A_RESTORE_GEOMETRY]);
		XDeleteProperty(wm.display, c->window, wm.atoms[A_ORDER]);
		XDeleteProperty(wm.display, c->window, wm.atoms[A_WM_DESKTOP]);
		XUngrabServer(wm.display);
	}

	wm_draw_destroy_window(c->frame);
	free_client(c);
	window_client_sync_visibility();
	window_client_publish_list();

	if (focused)
		window_client_focus_topmost();
	taskbar_draw();
}

void
window_client_cleanup(void)
{
	struct client *c = NULL;

	window_frame_cancel_drag(NULL);
	XGrabServer(wm.display);

	/*
	 * Keep ancestors available until every window's visibility is restored.
	 */
	for (c = wm.clients; c != NULL; c = c->next) {
		int minimized;

		minimized = window_client_minimized(c);
		detach_frame(c, minimized);
		if (!minimized)
			XMapWindow(wm.display, c->window);

		wm_draw_destroy_window(c->frame);
	}

	for (c = wm.clients; c != NULL;) {
		struct client *next = NULL;

		next = c->next;
		focus_cycle_forget(c);

		free_client(c);
		c = next;
	}

	wm.clients = wm.focused = NULL;
	XSetInputFocus(
		wm.display, PointerRoot, RevertToPointerRoot, CurrentTime);
	XUngrabServer(wm.display);
}
