#include "events.h"
#include "wm.h"

static void	property_changed(struct client *, Atom);
static void	button_pressed(struct client *, XButtonEvent *);
static void	pointer_entered(struct client *, XCrossingEvent *);
static void	focus_changed(struct client *, XFocusChangeEvent *);
static void	screen_resized(XConfigureEvent *);

/* Title/icon updates use narrow redraws. Other hints refresh metadata. */
static void
property_changed(struct client *c, Atom property)
{
	if (property == XA_WM_NAME || property == wm.atoms[A_NAME]) {
		window_client_refresh_title(c);
		window_frame_draw(c);
		taskbar_draw_client(c);
		return;
	}

	if (property == wm.atoms[A_ICON]) {
		taskbar_icon_update(c);
		taskbar_draw_client(c);
		return;
	}

	if (!window_client_refresh_property(c, property))
		return;

	if (property == XA_WM_NORMAL_HINTS || property == XA_WM_COMMAND ||
		property == XA_WM_CLIENT_MACHINE || property == wm.atoms[A_PID])
		return;

	if (property == XA_WM_CLASS || property == XA_WM_HINTS) {
		taskbar_icon_update(c);
		taskbar_draw_client(c);
		return;
	}

	if (property == wm.atoms[A_PROTOCOLS]) {
		window_frame_draw(c);
		return;
	}

	if (property == XA_WM_TRANSIENT_FOR) {
		window_client_sync_visibility();

		if (wm.focused != NULL && !window_client_visible(wm.focused))
			window_client_focus_topmost();
	}

	window_frame_draw(c);
	taskbar_draw();
}

static void
button_pressed(struct client *c, XButtonEvent *event)
{
	if (c == NULL || event->window == c->window)
		window_frame_reset_click();

	if (event->window == wm.taskbar) {
		taskbar_press(event);
		return;
	}

	if (event->window == wm.root && event->button == Button3) {
		owm_menu_open(event->x_root, event->y_root, event->time);
		return;
	}

	if (c == NULL)
		return;

	window_client_focus(c, event->time);

	if (event->window == c->window) {
		XAllowEvents(wm.display, ReplayPointer, event->time);
		return;
	}

	if (event->button == Button3 && event->y >= BORDER &&
		event->y < window_frame_top()) {
		owm_menu_open_window(
			c, event->x_root, event->y_root, event->time, 0);
		return;
	}

	window_frame_press(c, event);
}

static void
pointer_entered(struct client *c, XCrossingEvent *e)
{
	Window child, root;
	int rx, ry, wx, wy;
	unsigned int buttons, state;

	if (!wm.config.focus_follows_mouse || c == NULL || c == wm.focused ||
		!window_client_visible(c) || window_frame_drag_active())
		return;

	if (e->mode != NotifyNormal || e->detail == NotifyInferior)
		return;

	buttons = Button1Mask | Button2Mask | Button3Mask | Button4Mask |
		Button5Mask;

	if (e->state & buttons)
		return;

	/*
	 * Ignore stale crossings queued before a restack or a fast
	 * pointer move.
	 */

	if (XQueryPointer(wm.display, wm.root, &root, &child, &rx, &ry, &wx,
		&wy, &state) &&
		child == c->frame)
		window_client_focus_pointer(c, e->time);
}

static void
focus_changed(struct client *c, XFocusChangeEvent *e)
{
	if (c == NULL || c == wm.focused || !window_client_visible(c))
		return;

	if (e->mode != NotifyNormal || e->detail == NotifyPointer ||
		e->detail == NotifyPointerRoot || e->detail == NotifyDetailNone)
		return;

	/*
	 * Observe application requested focus without sending another focus
	 * request: replaying queued FocusIn events otherwise creates a
	 * feedback loop.
	 */
	window_client_record_focus(c);
}

static void
screen_resized(XConfigureEvent *e)
{
	struct client *p = NULL;

	if (e->window != wm.root ||
		(wm.width == e->width && wm.height == e->height))
		return;

	window_move_finish(1);
	wm.width = e->width;
	wm.height = e->height;
	owm_menu_close();
	desktop_update_workarea();
	taskbar_resize();
	desktop_icons_resize();

	for (p = wm.clients; p != NULL; p = p->next) {
		if (p->fullscreen) {
			struct geometry screen = {0};

			screen.x = 0;
			screen.y = 0;
			screen.w = wm.width;
			screen.h = wm.height;

			window_client_configure(p, screen, 0);
		} else if (p->max_h || p->max_v)
			window_client_maximize(p, p->max_h, p->max_v);

		/*
		 * Preserve ordinary window geometry. SDL can change the video
		 * mode before requesting fullscreen. Fitting windows here
		 * would overwrite the size and position it needs to restore
		 * later.
		 */
	}
}

void
wm_events_dispatch(XEvent *e)
{
	struct client *c = NULL;

	if (focus_cycle_event(e) || taskbar_tooltip_event(e))
		return;

	if (window_move_event(e) || owm_menu_event(e) || desktop_icons_event(e))
		return;

	c = window_client_find(e->xany.window);

	switch (e->type) {
	case MapRequest:
		c = window_client_find(e->xmaprequest.window);

		if (c != NULL) {
			window_client_minimize(c, 0);
			window_client_focus(c, CurrentTime);
		} else
			window_client_manage(e->xmaprequest.window);
		break;

	case ConfigureRequest:
		window_client_handle_configure_request(&e->xconfigurerequest);
		break;

	case DestroyNotify:
		c = window_client_find(e->xdestroywindow.window);

		if (c != NULL && c->window == e->xdestroywindow.window)
			window_client_unmanage(c, 1);
		break;

	case UnmapNotify:
		c = window_client_find(e->xunmap.window);

		if (c == NULL || c->window != e->xunmap.window ||
			(e->xunmap.event != c->window &&
				e->xunmap.send_event == 0))
			break;

		if (c->ignore_unmap && e->xunmap.send_event == 0) {
			c->ignore_unmap--;
			break;
		}

		window_client_unmanage(c, 0);
		break;

	case EnterNotify:
		pointer_entered(c, &e->xcrossing);
		break;

	case ButtonPress:
		button_pressed(c, &e->xbutton);
		break;

	case ButtonRelease:
		window_frame_release(&e->xbutton);
		break;

	case KeyPress:
		owm_shortcuts_press(&e->xkey);
		break;

	case MappingNotify:
		XRefreshKeyboardMapping(&e->xmapping);

		if (e->xmapping.request == MappingKeyboard ||
			e->xmapping.request == MappingModifier)
			owm_shortcuts_grab();
		break;

	case MotionNotify:
		window_frame_motion(&e->xmotion);
		break;

	case Expose:
		if (e->xexpose.count)
			break;

		if (e->xexpose.window == wm.taskbar)
			taskbar_draw();
		else if (c != NULL)
			window_frame_draw(c);
		break;

	case PropertyNotify:
		if (c == NULL || e->xproperty.window != c->window)
			break;

		property_changed(c, e->xproperty.atom);
		focus_cycle_draw();
		break;

	case ClientMessage:
		wm_protocol_message(&e->xclient);
		break;

	case FocusIn:
		focus_changed(c, &e->xfocus);
		break;

	case ConfigureNotify:
		screen_resized(&e->xconfigure);
		if (e->xconfigure.window == wm.root)
			focus_cycle_draw();
		break;

	case SelectionClear:
		if (e->xselectionclear.selection == wm.wm_selection)
			wm.running = 0;
		break;

	default:
		break;
	}
}
