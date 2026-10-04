#ifndef OWM_EVENTS_H
#define OWM_EVENTS_H

#include <X11/Xlib.h>

/* Dispatch one borrowed X11 event to the WM and UI handlers. */
void	wm_events_dispatch(XEvent *);

#endif
