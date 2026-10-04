#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "protocol.h"
#include "wm.h"

static int	state_action(int, long);
static void	send_frame_extents(Window);
static void	set_skip_taskbar(struct client *, int);
static void	request_window_state(struct client *, XClientMessageEvent *);
static void	update_state(Window, const Atom *, int, const Atom *, int);

static char *atom_names[ATOM_COUNT] = {
	"WM_PROTOCOLS",			/* A_PROTOCOLS */
	"WM_DELETE_WINDOW",		/* A_DELETE */
	"WM_TAKE_FOCUS",		/* A_TAKE_FOCUS */
	"WM_STATE",			/* A_STATE */
	"WM_CHANGE_STATE",		/* A_CHANGE_STATE */
	"UTF8_STRING",			/* A_UTF8 */
	"_NET_WM_NAME",			/* A_NAME */
	"_NET_WM_PID",			/* A_PID */
	"_NET_ACTIVE_WINDOW",		/* A_ACTIVE */
	"_NET_CLIENT_LIST",		/* A_CLIENT_LIST */
	"_NET_WORKAREA",		/* A_WORKAREA */
	"_NET_WM_STATE",		/* A_NET_STATE */
	"_NET_WM_STATE_FULLSCREEN",	/* A_FULLSCREEN */
	"_NET_WM_STATE_MAXIMIZED_VERT", /* A_MAX_VERT */
	"_NET_WM_STATE_MAXIMIZED_HORZ", /* A_MAX_HORZ */
	"_NET_WM_STATE_HIDDEN",		/* A_HIDDEN */
	"_NET_WM_STATE_SKIP_TASKBAR",	/* A_SKIP_TASKBAR */
	"_NET_WM_WINDOW_TYPE",		/* A_TYPE */
	"_NET_WM_WINDOW_TYPE_DOCK",	/* A_TYPE_DOCK */
	"_NET_WM_WINDOW_TYPE_DESKTOP",	/* A_TYPE_DESKTOP */
	"_NET_WM_WINDOW_TYPE_MENU",	/* A_TYPE_MENU */
	"_NET_WM_WINDOW_TYPE_TOOLTIP",	/* A_TYPE_TOOLTIP */
	"_NET_SUPPORTED",		/* A_SUPPORTED */
	"_NET_SUPPORTING_WM_CHECK",	/* A_CHECK */
	"_NET_FRAME_EXTENTS",		/* A_FRAME_EXTENTS */
	"_NET_REQUEST_FRAME_EXTENTS",	/* A_REQUEST_EXTENTS */
	"_NET_CLOSE_WINDOW",		/* A_CLOSE */
	"_NET_NUMBER_OF_DESKTOPS",	/* A_DESKTOP_COUNT */
	"_NET_CURRENT_DESKTOP",		/* A_CURRENT_DESKTOP */
	"_NET_DESKTOP_GEOMETRY",	/* A_DESKTOP_GEOMETRY */
	"_NET_DESKTOP_VIEWPORT",	/* A_DESKTOP_VIEWPORT */
	"_NET_WM_STRUT",		/* A_STRUT */
	"_NET_WM_STRUT_PARTIAL",	/* A_STRUT_PARTIAL */
	"_OWM_ORDER",			/* A_ORDER */
	"_OWM_RESTORE_GEOMETRY",	/* A_RESTORE_GEOMETRY */
	"_NET_WM_DESKTOP",		/* A_WM_DESKTOP */
	"_NET_DESKTOP_NAMES",		/* A_DESKTOP_NAMES */
	"_OWM_CURRENT_DESKTOP",		/* A_LAST_DESKTOP */
	"_NET_WM_WINDOW_TYPE_DIALOG",	/* A_TYPE_DIALOG */
	"_OWM_CONTEXT_HELP",		/* A_CONTEXT_HELP */
	"_NET_WM_ICON",			/* A_ICON */
};

int
wm_protocol_init(void)
{
	XEvent manager = {0};
	size_t i;
	static const int properties[] = {A_NAME, A_PID, A_ACTIVE, A_CLIENT_LIST,
		A_WORKAREA, A_NET_STATE, A_FULLSCREEN, A_MAX_VERT, A_MAX_HORZ,
		A_HIDDEN, A_SKIP_TASKBAR, A_TYPE, A_TYPE_DIALOG, A_TYPE_DOCK,
		A_TYPE_DESKTOP, A_TYPE_MENU, A_TYPE_TOOLTIP, A_CHECK,
		A_FRAME_EXTENTS, A_REQUEST_EXTENTS, A_CLOSE, A_DESKTOP_COUNT,
		A_CURRENT_DESKTOP, A_DESKTOP_GEOMETRY, A_DESKTOP_VIEWPORT,
		A_WM_DESKTOP, A_DESKTOP_NAMES, A_ICON};
	Atom supported[sizeof(properties) / sizeof(*properties)];
	char selection[32];

	if (XInternAtoms(wm.display, atom_names, ATOM_COUNT, False, wm.atoms) ==
		0) {
		warnx("cannot initialize X11 atoms");
		return 0;
	}

	snprintf(selection, sizeof(selection), "WM_S%d", wm.screen);
	wm.wm_selection = XInternAtom(wm.display, selection, False);
	XSetSelectionOwner(
		wm.display, wm.wm_selection, wm.support, CurrentTime);

	manager.xclient.type = ClientMessage;
	manager.xclient.window = wm.root;
	manager.xclient.message_type =
		XInternAtom(wm.display, "MANAGER", False);
	manager.xclient.format = 32;
	manager.xclient.data.l[0] = CurrentTime;
	manager.xclient.data.l[1] = (long)wm.wm_selection;
	manager.xclient.data.l[2] = (long)wm.support;
	XSendEvent(wm.display, wm.root, False, StructureNotifyMask, &manager);

	for (i = 0; i < sizeof(properties) / sizeof(*properties); i++)
		supported[i] = wm.atoms[properties[i]];

	XChangeProperty(wm.display, wm.root, wm.atoms[A_SUPPORTED], XA_ATOM, 32,
		PropModeReplace, (unsigned char *)supported,
		(int)(sizeof(supported) / sizeof(*supported)));
	XChangeProperty(wm.display, wm.root, wm.atoms[A_CHECK], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *)&wm.support, 1);
	XChangeProperty(wm.display, wm.support, wm.atoms[A_CHECK], XA_WINDOW,
		32, PropModeReplace, (unsigned char *)&wm.support, 1);
	XChangeProperty(wm.display, wm.support, wm.atoms[A_NAME],
		wm.atoms[A_UTF8], 8, PropModeReplace, (unsigned char *)"owm",
		sizeof("owm") - 1);

	return 1;
}

void
wm_protocol_cleanup(void)
{
	size_t i;
	int properties[] = {A_SUPPORTED, A_CHECK, A_CLIENT_LIST, A_ACTIVE,
		A_WORKAREA, A_DESKTOP_COUNT, A_CURRENT_DESKTOP,
		A_DESKTOP_GEOMETRY, A_DESKTOP_VIEWPORT, A_DESKTOP_NAMES};

	for (i = 0; i < sizeof(properties) / sizeof(*properties); i++)
		XDeleteProperty(wm.display, wm.root, wm.atoms[properties[i]]);
}

void
wm_protocol_frame_extents(Window window, int fullscreen)
{
	unsigned long extents[4] = {0};

	if (!fullscreen) {
		extents[0] = extents[1] = extents[3] = BORDER;
		extents[2] = (unsigned long)window_frame_top();
	}
	wm_property_set_cardinals(
		window, wm.atoms[A_FRAME_EXTENTS], extents, 4);
}

/* Replace only owned atoms, preserving all other client state. */
static void
update_state(Window window, const Atom *owned, int owned_count,
	const Atom *added, int added_count)
{
	Atom *states = NULL;
	unsigned char *raw = NULL;
	unsigned long i, n;
	int count, j;

	raw = wm_property_get(window, wm.atoms[A_NET_STATE], XA_ATOM, 32, &n);
	states = allocate_array(n + owned_count, sizeof(*states));
	count = 0;
	for (i = 0; raw != NULL && i < n; i++) {
		for (j = 0; j < owned_count; j++)
			if (((Atom *)raw)[i] == owned[j])
				break;
		if (j == owned_count)
			states[count++] = ((Atom *)raw)[i];
	}
	for (j = 0; j < added_count; j++)
		states[count++] = added[j];
	XChangeProperty(wm.display, window, wm.atoms[A_NET_STATE], XA_ATOM, 32,
		PropModeReplace, (unsigned char *)states, count);
	if (raw != NULL)
		XFree(raw);
	free(states);
}

void
window_client_publish_list(void)
{
	struct client *c = NULL;
	Window *list = NULL;
	int count = 0, i;

	for (c = wm.clients; c != NULL; c = c->next) {
		if (count == INT_MAX)
			errx(1, "too many client windows");
		count++;
	}

	list = allocate_array((size_t)count + 1, sizeof(*list));
	i = 0;

	for (c = wm.clients; c != NULL; c = c->next)
		list[i++] = c->window;

	XChangeProperty(wm.display, wm.root, wm.atoms[A_CLIENT_LIST], XA_WINDOW,
		32, PropModeReplace, (unsigned char *)list, count);
	free(list);
}

void
window_client_send_configure_notify(struct client *c)
{
	XEvent e = {0};

	e.xconfigure.type = ConfigureNotify;
	e.xconfigure.display = wm.display;
	e.xconfigure.event = e.xconfigure.window = c->window;
	e.xconfigure.x = c->g.x + (c->fullscreen ? 0 : BORDER);
	e.xconfigure.y = c->g.y + (c->fullscreen ? 0 : window_frame_top());
	e.xconfigure.width = c->g.w;
	e.xconfigure.height = c->g.h;
	e.xconfigure.border_width = 0;
	XSendEvent(wm.display, c->window, False, StructureNotifyMask, &e);
}

void
window_client_publish_state(struct client *c)
{
	Atom owned[4], states[4];
	long state[2] = {0};
	int count, hidden;

	hidden = window_client_minimized(c);
	state[0] = hidden ? IconicState : NormalState;
	state[1] = None;
	XChangeProperty(wm.display, c->window, wm.atoms[A_STATE],
		wm.atoms[A_STATE], 32, PropModeReplace, (unsigned char *)state,
		2);
	owned[0] = wm.atoms[A_MAX_VERT];
	owned[1] = wm.atoms[A_MAX_HORZ];
	owned[2] = wm.atoms[A_HIDDEN];
	owned[3] = wm.atoms[A_FULLSCREEN];
	count = 0;
	if (c->fullscreen)
		states[count++] = wm.atoms[A_FULLSCREEN];
	if (c->max_h)
		states[count++] = wm.atoms[A_MAX_HORZ];
	if (c->max_v)
		states[count++] = wm.atoms[A_MAX_VERT];
	if (hidden)
		states[count++] = wm.atoms[A_HIDDEN];
	update_state(c->window, owned, 4, states, count);
}

void
wm_protocol_send(struct client *c, Atom atom, Time time)
{
	XEvent e = {0};

	e.xclient.type = ClientMessage;
	e.xclient.window = c->window;
	e.xclient.message_type = wm.atoms[A_PROTOCOLS];
	e.xclient.format = 32;
	e.xclient.data.l[0] = (long)atom;
	e.xclient.data.l[1] = (long)time;
	XSendEvent(wm.display, c->window, False, NoEventMask, &e);
}

static int
state_action(int current, long action)
{
	return action == 2 ? !current : action == 1;
}

static void
send_frame_extents(Window window)
{
	struct client *client = NULL;
	int fullscreen;

	client = window_client_find(window);
	fullscreen = (client != NULL)
		? client->fullscreen
		: wm_property_has_atom(window, wm.atoms[A_NET_STATE],
			wm.atoms[A_FULLSCREEN]);

	wm_protocol_frame_extents(window, fullscreen);
}

static void
request_window_state(struct client *c, XClientMessageEvent *e)
{
	int fullscreen, h, i, v;

	h = c->max_h;
	v = c->max_v;
	fullscreen = c->fullscreen;

	for (i = 1; i <= 2; i++) {
		Atom a;

		a = (Atom)e->data.l[i];

		if (i == 2 && a == (Atom)e->data.l[1])
			continue;

		if (a == wm.atoms[A_FULLSCREEN])
			fullscreen = state_action(fullscreen, e->data.l[0]);

		if (a == wm.atoms[A_MAX_HORZ])
			h = state_action(h, e->data.l[0]);

		if (a == wm.atoms[A_MAX_VERT])
			v = state_action(v, e->data.l[0]);

		if (a == wm.atoms[A_SKIP_TASKBAR])
			set_skip_taskbar(
				c, state_action(c->skip_taskbar, e->data.l[0]));
	}

	window_state_request_fullscreen(c, fullscreen);

	if (h != c->max_h || v != c->max_v)
		window_client_maximize(c, h, v);
	taskbar_draw();
}

void
wm_protocol_message(XClientMessageEvent *e)
{
	struct client *c = NULL;

	if (e->format != 32)
		return;

	if (e->message_type == wm.atoms[A_CURRENT_DESKTOP]) {
		if ((unsigned long)e->data.l[0] <
			(unsigned long)wm.config.desktops)
			desktop_switch((int)e->data.l[0], (Time)e->data.l[1]);
		return;
	}

	if (e->message_type == wm.atoms[A_REQUEST_EXTENTS]) {
		send_frame_extents(e->window);
		return;
	}

	c = window_client_find(e->window);

	if (c == NULL)
		return;

	if (e->message_type == wm.atoms[A_ACTIVE]) {
		window_client_focus(c, (Time)e->data.l[1]);
		return;
	}

	if (e->message_type == wm.atoms[A_WM_DESKTOP]) {
		unsigned long desktop;

		desktop = (unsigned long)e->data.l[0] & 0xffffffffUL;

		if (desktop == 0xffffffffUL ||
			desktop < (unsigned long)wm.config.desktops)
			desktop_send(c,
				desktop == 0xffffffffUL ? -1 : (int)desktop,
				CurrentTime);

		return;
	}

	if (e->message_type == wm.atoms[A_CLOSE]) {
		window_client_close(c, (Time)e->data.l[0]);
		return;
	}

	if (e->message_type == wm.atoms[A_CHANGE_STATE] &&
		e->data.l[0] == IconicState) {
		window_state_request_iconify(c);
		return;
	}

	if (e->message_type == wm.atoms[A_NET_STATE] && e->data.l[0] >= 0 &&
		e->data.l[0] <= 2) {
		request_window_state(c, e);
	}
}

/* Preserve other state atoms when changing taskbar visibility. */
static void
set_skip_taskbar(struct client *c, int skip)
{
	Atom atom;

	atom = wm.atoms[A_SKIP_TASKBAR];
	update_state(c->window, &atom, 1, &atom, skip ? 1 : 0);
	c->skip_taskbar = skip;
}
