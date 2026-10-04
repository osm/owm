#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "wm.h"

static void	replace(char **, const char *);
static void	refresh_class(struct client *);
static void	refresh_pid(struct client *);
static void	refresh_command(struct client *);
static void	refresh_transient(struct client *);
static void	refresh_type(struct client *);
static void	refresh_size_hints(struct client *);
static void	refresh_input_hint(struct client *);
static void	refresh_protocols(struct client *);

static void
replace(char **target, const char *value)
{
	free(*target);
	*target = copy_string(value);
}

void
window_client_refresh_title(struct client *c)
{
	unsigned long n;
	char *legacy = NULL;
	unsigned char *name = NULL;

	name = wm_property_get(
		c->window, wm.atoms[A_NAME], wm.atoms[A_UTF8], 8, &n);

	if (name != NULL) {
		replace(&c->title, (char *)name);
		XFree(name);
		return;
	}

	XFetchName(wm.display, c->window, &legacy);
	replace(&c->title, (legacy != NULL) ? legacy : "Untitled");

	if (legacy != NULL)
		XFree(legacy);
}

static void
refresh_class(struct client *c)
{
	XClassHint hint = {0};

	XGetClassHint(wm.display, c->window, &hint);
	replace(&c->instance, hint.res_name);
	replace(&c->class_name, hint.res_class);
	if (hint.res_name != NULL)
		XFree(hint.res_name);
	if (hint.res_class != NULL)
		XFree(hint.res_class);
}

static void
refresh_pid(struct client *c)
{
	unsigned long n;
	unsigned char *pid = NULL;

	pid = wm_property_get(c->window, wm.atoms[A_PID], XA_CARDINAL, 32, &n);
	c->pid = pid != NULL && n ? ((unsigned long *)pid)[0] : 0;
	if (pid != NULL)
		XFree(pid);
}

static void
refresh_command(struct client *c)
{
	char *command = NULL;

	command = session_command_capture(c);
	if (command != NULL) {
		free(c->command);
		c->command = command;
	}
}

static void
refresh_transient(struct client *c)
{
	c->transient = None;
	XGetTransientForHint(wm.display, c->window, &c->transient);
	if (c->transient == c->window)
		c->transient = None;
}

static void
refresh_type(struct client *c)
{
	Atom *types = NULL;
	unsigned long count, i;

	types = (Atom *)wm_property_get(
		c->window, wm.atoms[A_TYPE], XA_ATOM, 32, &count);
	c->dialog = 0;
	c->skip_taskbar = wm_property_has_atom(
		c->window, wm.atoms[A_NET_STATE], wm.atoms[A_SKIP_TASKBAR]);
	for (i = 0; types != NULL && i < count; i++) {
		if (types[i] == wm.atoms[A_TYPE_DIALOG])
			c->dialog = 1;
		if (types[i] == wm.atoms[A_TYPE_MENU] ||
			types[i] == wm.atoms[A_TYPE_TOOLTIP])
			c->skip_taskbar = 1;
	}
	if (types != NULL)
		XFree(types);
}

static void
refresh_size_hints(struct client *c)
{
	long supplied;

	memset(&c->hints, 0, sizeof(c->hints));
	XGetWMNormalHints(wm.display, c->window, &c->hints, &supplied);
}

static void
refresh_input_hint(struct client *c)
{
	XWMHints *hints = NULL;

	hints = XGetWMHints(wm.display, c->window);
	c->accepts_input = hints == NULL || (hints->flags & InputHint) == 0 ||
		hints->input;
	if (hints != NULL)
		XFree(hints);
}

static void
refresh_protocols(struct client *c)
{
	Atom *protocols = NULL;
	int count = 0, i;

	c->delete_window = c->take_focus = c->context_help = 0;
	if (XGetWMProtocols(wm.display, c->window, &protocols, &count) == 0)
		return;
	for (i = 0; i < count; i++) {
		if (protocols[i] == wm.atoms[A_CONTEXT_HELP])
			c->context_help = 1;
		if (protocols[i] == wm.atoms[A_DELETE])
			c->delete_window = 1;
		if (protocols[i] == wm.atoms[A_TAKE_FOCUS])
			c->take_focus = 1;
	}
	XFree(protocols);
}

int
window_client_refresh_property(struct client *c, Atom property)
{
	if (property == XA_WM_CLASS)
		refresh_class(c);
	else if (property == XA_WM_NORMAL_HINTS)
		refresh_size_hints(c);
	else if (property == XA_WM_HINTS)
		refresh_input_hint(c);
	else if (property == XA_WM_TRANSIENT_FOR)
		refresh_transient(c);
	else if (property == wm.atoms[A_PROTOCOLS])
		refresh_protocols(c);
	else if (property == wm.atoms[A_TYPE] ||
		property == wm.atoms[A_NET_STATE])
		refresh_type(c);
	else if (property == wm.atoms[A_PID]) {
		refresh_pid(c);
		refresh_command(c);
	} else if (property == XA_WM_COMMAND ||
		property == XA_WM_CLIENT_MACHINE)
		refresh_command(c);
	else
		return 0;

	return 1;
}

void
window_client_refresh(struct client *c)
{
	window_client_refresh_title(c);
	refresh_class(c);
	refresh_pid(c);
	refresh_command(c);
	refresh_transient(c);
	refresh_type(c);
	refresh_size_hints(c);
	refresh_input_hint(c);
	refresh_protocols(c);
}
