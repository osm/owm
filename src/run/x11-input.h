#ifndef OWM_X11_INPUT_H
#define OWM_X11_INPUT_H

#include <X11/Xlib.h>

/* Owns XIM/XIC until free. The display and window are borrowed. */
struct x11_input {
	Display	*display;
	Window	window;
	XIM	method;
	XIC	context;
	Atom	clipboard;
	Atom	paste;
	Atom	utf8;
};

int	x11_input_init(struct x11_input *, Display *, Window);
void	x11_input_free(struct x11_input *);
long	x11_input_events(const struct x11_input *);
int	x11_input_filter(struct x11_input *, XEvent *);
/* Returns the UTF8 byte count, or negative one if the buffer is too small. */
int	x11_input_key(struct x11_input *, XKeyEvent *, char *, int, KeySym *);
void	x11_input_request_paste(struct x11_input *, int, Time);
/*
 * Returns owned UTF8 text (free with free), or NULL for ignored/pending paste.
 */
char	*x11_input_paste(struct x11_input *, XSelectionEvent *);

#endif
