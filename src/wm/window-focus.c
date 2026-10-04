#include "wm.h"

static void	raise_group(struct client *, int);
static void	focus(struct client *, Time, int);

static void
raise_group(struct client *c, int depth)
{
	struct client *child = NULL;

	XRaiseWindow(wm.display, c->frame);
	if (depth >= MAX_TRANSIENT_DEPTH)
		return;

	/*
	 * Raise descendants after their ancestors, with a depth limit for
	 * broken hints.
	 */

	for (child = wm.clients; child != NULL; child = child->next)
		if (child->transient == c->window &&
			window_client_visible(child))
			raise_group(child, depth + 1);
}

void
window_client_restack(void)
{
	struct client *fullscreen = NULL, *p = NULL;
	int depth;

	XRaiseWindow(wm.display, wm.taskbar);

	/*
	 * A focused fullscreen window and its transient dialogs cover the
	 * taskbar. Focusing another application leaves fullscreen intact behind
	 * that app.
	 */
	p = wm.focused;
	fullscreen = NULL;

	for (depth = 0; p != NULL && depth < MAX_TRANSIENT_DEPTH; depth++) {
		if (p->fullscreen && window_client_visible(p))
			fullscreen = p;
		p = window_client_find(p->transient);
	}

	if (fullscreen == NULL)
		return;

	raise_group(fullscreen, 0);

	if (wm.focused != fullscreen)
		raise_group(wm.focused, 0);
}

/* FocusIn updates bookkeeping without requesting focus again. */
void
window_client_record_focus(struct client *c)
{
	struct client *old = NULL;
	Window active;

	old = wm.focused;
	wm.focused = c;
	focus_cycle_record(c);

	if (c != NULL)
		wm.desktop_focus[wm.current_desktop] = c->window;
	window_client_restack();
	focus_cycle_draw();

	/* A removed client clears wm.focused before choosing its successor. */
	if (c != NULL && old == c)
		return;

	active = (c != NULL) ? c->window : None;

	XChangeProperty(wm.display, wm.root, wm.atoms[A_ACTIVE], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *)&active, 1);

	if (old != NULL && old != c)
		window_frame_draw(old);

	if (c != NULL)
		window_frame_draw(c);
	taskbar_focus_changed(old);
}

static void
focus(struct client *c, Time time, int raise)
{
	struct client *ancestor = NULL;
	int depth;

	if (wm.focused != c)
		window_move_finish(1);

	if (c != NULL && !desktop_contains(c))
		desktop_switch(desktop_for_client(c), time);

	ancestor = c;

	for (depth = 0; ancestor != NULL && depth < MAX_TRANSIENT_DEPTH;
		depth++) {
		if (ancestor->minimized)
			window_client_minimize(ancestor, 0);
		ancestor = window_client_find(ancestor->transient);
	}

	if (c != NULL) {
		if (raise)
			raise_group(c, 0);

		if (c->accepts_input)
			XSetInputFocus(wm.display, c->window,
				RevertToPointerRoot, time);
		else
			XSetInputFocus(
				wm.display, wm.root, RevertToPointerRoot, time);

		if (c->take_focus)
			wm_protocol_send(c, wm.atoms[A_TAKE_FOCUS], time);
	} else
		XSetInputFocus(wm.display, wm.root, RevertToPointerRoot, time);
	window_client_record_focus(c);
}

void
window_client_focus(struct client *c, Time time)
{
	focus(c, time, 1);
}

void
window_client_focus_pointer(struct client *c, Time time)
{
	if (!focus_cycle_active() && c != NULL && c != wm.focused &&
		window_client_visible(c))
		focus(c, time, 0);
}

void
window_client_focus_topmost(void)
{
	struct client *candidate = NULL;
	Window *children = NULL, parent, root;
	unsigned int count;

	if (XQueryTree(
		wm.display, wm.root, &root, &parent, &children, &count)) {
		unsigned int i;

		for (i = count; i > 0; i--) {
			struct client *c = NULL;

			c = window_client_find(children[i - 1]);

			if (c != NULL && window_client_visible(c)) {
				candidate = c;
				break;
			}
		}

		if (children != NULL)
			XFree(children);
	}

	window_client_focus(candidate, CurrentTime);
}
