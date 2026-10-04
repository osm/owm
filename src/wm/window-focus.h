#ifndef OWM_WINDOW_FOCUS_H
#define OWM_WINDOW_FOCUS_H

#include <X11/Xlib.h>

struct client;

void	window_client_restack(void);
void	window_client_record_focus(struct client *);
void	window_client_focus(struct client *, Time);
void	window_client_focus_pointer(struct client *, Time);
void	window_client_focus_topmost(void);

#endif
