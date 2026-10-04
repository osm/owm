#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <err.h>
#include <string.h>

#include "memory.h"
#include "text-input.h"
#include "x11-input.h"

int
x11_input_init(struct x11_input *input, Display *display, Window window)
{
	int local;

	memset(input, 0, sizeof(*input));
	input->display = display;
	input->window = window;
	input->clipboard = XInternAtom(display, "CLIPBOARD", False);
	input->paste = XInternAtom(display, "_OWM_RUN_PASTE", False);
	input->utf8 = XInternAtom(display, "UTF8_STRING", False);

	for (local = 0; local < 2; local++) {
		XSetLocaleModifiers(local ? "@im=none" : "");
		input->method = XOpenIM(input->display, NULL, NULL, NULL);
		if (input->method == NULL)
			continue;
		input->context = XCreateIC(input->method, XNInputStyle,
			XIMPreeditNothing | XIMStatusNothing, XNClientWindow,
			input->window, XNFocusWindow, input->window,
			(char *)NULL);
		if (input->context != NULL)
			return 1;
		XCloseIM(input->method);
		input->method = NULL;
	}
	warnx("cannot initialize text input");

	return 0;
}

char *
x11_input_paste(struct x11_input *input, XSelectionEvent *event)
{
	XTextProperty text = {0};
	unsigned long i, left;
	int count, status;
	char **strings = NULL;
	char *result = NULL;

	if (event->requestor != input->window ||
		(event->selection != input->clipboard &&
			event->selection != XA_PRIMARY) ||
		(event->target != input->utf8 && event->target != XA_STRING))
		return NULL;
	if (event->property == None) {
		if (event->target == input->utf8)
			XConvertSelection(input->display, event->selection,
				XA_STRING, input->paste, input->window,
				event->time);
		return NULL;
	}
	if (event->property != input->paste)
		return NULL;

	status = XGetWindowProperty(input->display, input->window, input->paste,
		0, TEXT_INPUT_SIZE / 4, True, event->target, &text.encoding,
		&text.format, &text.nitems, &left, &text.value);
	XDeleteProperty(input->display, input->window, input->paste);
	if (status != Success || text.encoding != event->target ||
		text.format != 8 || left)
		goto done;

	for (i = 0; i < text.nitems; i++) {
		if (text.value[i] >= 32 && text.value[i] != 127)
			continue;
		goto done;
	}

	/* XA_STRING uses Latin 1. The entry and launched argv use UTF8. */
	if (Xutf8TextPropertyToTextList(
		input->display, &text, &strings, &count) == Success &&
		count == 1)
		result = copy_string(strings[0]);
done:
	if (strings != NULL)
		XFreeStringList(strings);
	if (text.value != NULL)
		XFree(text.value);

	return result;
}

void
x11_input_free(struct x11_input *input)
{
	if (input->context != NULL)
		XDestroyIC(input->context);
	if (input->method != NULL)
		XCloseIM(input->method);
	input->context = NULL;
	input->method = NULL;
}

long
x11_input_events(const struct x11_input *input)
{
	long events = 0;

	XGetICValues(input->context, XNFilterEvents, &events, (char *)NULL);

	return events | FocusChangeMask;
}

int
x11_input_filter(struct x11_input *input, XEvent *event)
{
	if (XFilterEvent(event, input->window))
		return 1;
	switch (event->type) {
	case FocusIn:
		XSetICFocus(input->context);
		break;
	case FocusOut:
		XUnsetICFocus(input->context);
		break;
	case MappingNotify:
		XRefreshKeyboardMapping(&event->xmapping);
		break;
	default:
		break;
	}

	return 0;
}

int
x11_input_key(struct x11_input *input, XKeyEvent *event, char *text,
	int capacity, KeySym *key)
{
	Status status;
	int length;

	length = Xutf8LookupString(
		input->context, event, text, capacity, key, &status);
	if (status == XBufferOverflow)
		return -1;
	if (status != XLookupKeySym && status != XLookupBoth)
		*key = NoSymbol;

	return length;
}

void
x11_input_request_paste(struct x11_input *input, int primary, Time time)
{
	XConvertSelection(input->display,
		primary ? XA_PRIMARY : input->clipboard, input->utf8,
		input->paste, input->window, time);
}
