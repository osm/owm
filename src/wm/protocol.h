#ifndef OWM_PROTOCOL_H
#define OWM_PROTOCOL_H

#include <X11/Xlib.h>

struct client;

enum {
	A_PROTOCOLS,
	A_DELETE,
	A_TAKE_FOCUS,
	A_STATE,
	A_CHANGE_STATE,
	A_UTF8,
	A_NAME,
	A_PID,
	A_ACTIVE,
	A_CLIENT_LIST,
	A_WORKAREA,
	A_NET_STATE,
	A_FULLSCREEN,
	A_MAX_VERT,
	A_MAX_HORZ,
	A_HIDDEN,
	A_SKIP_TASKBAR,
	A_TYPE,
	A_TYPE_DOCK,
	A_TYPE_DESKTOP,
	A_TYPE_MENU,
	A_TYPE_TOOLTIP,
	A_SUPPORTED,
	A_CHECK,
	A_FRAME_EXTENTS,
	A_REQUEST_EXTENTS,
	A_CLOSE,
	A_DESKTOP_COUNT,
	A_CURRENT_DESKTOP,
	A_DESKTOP_GEOMETRY,
	A_DESKTOP_VIEWPORT,
	A_STRUT,
	A_STRUT_PARTIAL,
	A_ORDER,
	A_RESTORE_GEOMETRY,
	A_WM_DESKTOP,
	A_DESKTOP_NAMES,
	A_LAST_DESKTOP,
	A_TYPE_DIALOG,
	A_CONTEXT_HELP,
	A_ICON,
	ATOM_COUNT
};

int	wm_protocol_init(void);
void	wm_protocol_cleanup(void);
void	wm_protocol_message(XClientMessageEvent *);
void	wm_protocol_frame_extents(Window, int);
void	wm_protocol_send(struct client *, Atom, Time);

void	window_client_publish_list(void);
void	window_client_publish_state(struct client *);
void	window_client_send_configure_notify(struct client *);

#endif
