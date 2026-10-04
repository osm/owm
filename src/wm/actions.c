#include <err.h>
#include <stdlib.h>

#include "actions.h"
#include "process.h"
#include "wm.h"

int
owm_action_execute(const char *text, unsigned int modifiers, Time time)
{
	struct client *c = NULL;
	enum action action;
	int desktop = 0;

	action = owm_action_parse(text, &desktop);
	if (action != WINDOW_FOCUS_NEXT && action != WINDOW_FOCUS_PREV)
		focus_cycle_finish();
	c = wm.focused;

	switch (action) {
	case WINDOW_MENU:
		if (c != NULL)
			owm_menu_open_window(c, c->g.x + BORDER,
				c->g.y + window_frame_top(), time, 1);
		break;

	case WINDOW_MOVE_RESIZE:
		return window_move_begin(c, time);

	case WINDOW_CLOSE:
		if (c != NULL)
			window_client_close(c, time);
		break;

	case WINDOW_MAXIMIZE:
		if (c != NULL) {
			int maximize;

			maximize = !(c->max_h || c->max_v);

			window_client_maximize(c, maximize, maximize);
		}
		break;

	case WINDOW_FULLSCREEN:
		if (c != NULL)
			window_client_fullscreen(c, !c->fullscreen);
		break;

	case WINDOW_STICKY:
		desktop_send(c,
			desktop_for_client(c) == -1 ? wm.current_desktop : -1,
			time);
		break;

	case WINDOW_MINIMIZE:
		if (c != NULL)
			window_client_minimize(c, 1);
		break;

	case WINDOW_FOCUS_NEXT:
		focus_cycle_begin(modifiers, time);
		focus_cycle_step(0, time);
		break;

	case WINDOW_FOCUS_PREV:
		focus_cycle_begin(modifiers, time);
		focus_cycle_step(1, time);
		break;

	case SESSION_SAVE:
		return owm_session_save();

	case SESSION_RESTART: {
		char desktop[16];

		snprintf(desktop, sizeof(desktop), "%d", wm.current_desktop);
		if (setenv("OWM_RESTART_DESKTOP", desktop, 1) < 0) {
			warn("prepare restart");
			return 0;
		}

		wm.restart = 1;
		wm.running = 0;
		break;
	}

	case SESSION_SAVE_EXIT:
		if (!owm_session_save())
			return 0;

		wm.running = 0;
		break;

	case SESSION_EXIT:
		wm.running = 0;
		break;

	case DESKTOP_NEXT:
		desktop_switch(desktop_offset(wm.current_desktop, 1), time);
		break;

	case DESKTOP_PREV:
		desktop_switch(desktop_offset(wm.current_desktop, -1), time);
		break;

	case WINDOW_SEND_NEXT:
		desktop_send(c, desktop_offset(wm.current_desktop, 1), time);
		break;

	case WINDOW_SEND_PREV:
		desktop_send(c, desktop_offset(wm.current_desktop, -1), time);
		break;

	case DESKTOP_SWITCH:
		desktop_switch(desktop - 1, time);
		break;

	case WINDOW_SEND_TO_DESKTOP:
		desktop_send(c, desktop - 1, time);
		break;

	case EXEC:
		if (process_shell(ConnectionNumber(wm.display), text + 5) < 0) {
			warn("cannot launch %s", text + 5);
			return 0;
		}
		break;

	case UNBIND:
		break;

	default:
		return 0;
	}

	return 1;
}
