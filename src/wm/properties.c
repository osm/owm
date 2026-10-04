#include "wm.h"

unsigned char *
wm_property_get(Window window, Atom property, Atom type, int format,
	unsigned long *count)
{
	Atom actual;
	unsigned long after;
	int actual_format;
	unsigned char *data = NULL;

	*count = 0;

	if (XGetWindowProperty(wm.display, window, property, 0, MAX_TEXT / 4,
		False, type, &actual, &actual_format, count, &after,
		&data) != Success ||
		actual != type || actual_format != format || after) {
		if (data != NULL)
			XFree(data);
		*count = 0;
		return NULL;
	}

	return data;
}

int
wm_property_has_atom(Window window, Atom property, Atom value)
{
	unsigned long count, i;
	int found = 0;
	unsigned char *data = NULL;

	data = wm_property_get(window, property, XA_ATOM, 32, &count);

	for (i = 0; data != NULL && i < count; i++)
		if (((Atom *)data)[i] == value)
			found = 1;

	if (data != NULL)
		XFree(data);

	return found;
}

void
wm_property_set_cardinals(
	Window window, Atom property, const unsigned long *values, int n)
{
	XChangeProperty(wm.display, window, property, XA_CARDINAL, 32,
		PropModeReplace, (const unsigned char *)values, n);
}
