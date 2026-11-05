#include <ctype.h>
#include <string.h>

#include "actions.h"
#include "constraints.h"
#include "settings.h"

enum action
owm_action_parse(const char *text, int *desktop)
{
	size_t i;

	static const struct {
		const char	*name;
		enum action	action;
	} actions[] = {{"session_save_exit", SESSION_SAVE_EXIT},
		{"session_exit", SESSION_EXIT}, {"window_close", WINDOW_CLOSE},
		{"window_maximize", WINDOW_MAXIMIZE},
		{"window_fullscreen", WINDOW_FULLSCREEN},
		{"window_sticky", WINDOW_STICKY},
		{"window_minimize", WINDOW_MINIMIZE},
		{"window_menu", WINDOW_MENU},
		{"window_move_resize", WINDOW_MOVE_RESIZE},
		{"window_remember_position", WINDOW_REMEMBER_POSITION},
		{"window_forget_position", WINDOW_FORGET_POSITION},
		{"window_focus_next", WINDOW_FOCUS_NEXT},
		{"window_focus_prev", WINDOW_FOCUS_PREV},
		{"session_save", SESSION_SAVE},
		{"session_restart", SESSION_RESTART},
		{"desktop_next", DESKTOP_NEXT}, {"desktop_prev", DESKTOP_PREV},
		{"window_send_next", WINDOW_SEND_NEXT},
		{"window_send_prev", WINDOW_SEND_PREV}, {"none", UNBIND}};

	for (i = 0; i < sizeof(actions) / sizeof(*actions); i++)
		if (strcmp(text, actions[i].name) == 0)
			return actions[i].action;

	if (strncmp(text, "desktop_switch ", 15) == 0 &&
		parse_integer(text + 15, 1, MAX_DESKTOPS, desktop))
		return DESKTOP_SWITCH;

	if (strncmp(text, "window_send_to_desktop ", 23) == 0 &&
		parse_integer(text + 23, 1, MAX_DESKTOPS, desktop))
		return WINDOW_SEND_TO_DESKTOP;

	if (strncmp(text, "exec ", 5) == 0) {
		text += 5;

		while (isspace((unsigned char)*text))
			text++;

		if (*text != '\0')
			return EXEC;
	}

	return INVALID;
}

int
owm_action_menu_valid(const char *text)
{
	enum action type;
	int desktop;

	type = owm_action_parse(text, &desktop);

	return type == SESSION_SAVE || type == SESSION_SAVE_EXIT ||
		type == SESSION_EXIT || type == SESSION_RESTART || type == EXEC;
}
