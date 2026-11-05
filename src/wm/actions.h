#ifndef OWM_ACTIONS_H
#define OWM_ACTIONS_H

#include <X11/Xlib.h>

enum action {
	INVALID,
	WINDOW_CLOSE,
	WINDOW_MAXIMIZE,
	WINDOW_FULLSCREEN,
	WINDOW_STICKY,
	WINDOW_MINIMIZE,
	WINDOW_MENU,
	WINDOW_MOVE_RESIZE,
	WINDOW_REMEMBER_POSITION,
	WINDOW_FORGET_POSITION,
	WINDOW_FOCUS_NEXT,
	WINDOW_FOCUS_PREV,
	SESSION_SAVE,
	SESSION_SAVE_EXIT,
	SESSION_RESTART,
	SESSION_EXIT,
	DESKTOP_NEXT,
	DESKTOP_PREV,
	WINDOW_SEND_NEXT,
	WINDOW_SEND_PREV,
	DESKTOP_SWITCH,
	WINDOW_SEND_TO_DESKTOP,
	EXEC,
	UNBIND
};

/* Parsing and validation do not depend on WM state or open an X11 display. */
enum action	owm_action_parse(const char *, int *);
int		owm_action_menu_valid(const char *);
/* Execution takes held shortcut modifiers, or zero for nonkeyboard actions. */
int	owm_action_execute(const char *, unsigned int, Time);

#endif
