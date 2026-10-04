#ifndef OWM_H
#define OWM_H

#include <sys/types.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>

#include "config.h"
#include "constraints.h"
#include "font.h"
#include "geometry.h"
#include "image.h"
#include "protocol.h"
#include "ui-colors.h"
#include "window-focus.h"
#include "window-state.h"

#define BORDER 6

enum {
	TASKBAR_ICON_SIZE = 16
};

struct client;

struct client {
	/* The application owns window. The WM owns its enclosing frame. */
	Window		window;
	Window		frame;
	Window		transient;
	Cursor		frame_cursor;
	Pixmap		taskbar_icon;
	Pixmap		taskbar_icon_active;
	unsigned int	taskbar_icon_width;
	unsigned int	taskbar_icon_height;

	/* Read elsewhere. Modify through the window state API. */
	struct geometry	g; /* Requested frame origin and client size. */
	/* Normal geometry before maximize/fullscreen. */
	struct geometry	restore;
	/* Last geometry sent to X11. Avoids redundant work. */
	struct geometry	applied;
	int		configured;
	unsigned int	applied_decoration;

	int		original_border;
	/* Expected notifications from unmaps requested by the WM. */
	int		ignore_unmap;
	int		order;
	int	desktop; /* Zero based. Negative one means all desktops. */

	int		minimized;
	int		fullscreen;
	int		max_h;
	int		max_v;
	int		skip_taskbar;
	int		accepts_input;
	/* Fullscreen exit while on another desktop. */
	int		desktop_fullscreen;
	int		delete_window;
	int		take_focus;
	int		dialog;
	int		context_help;

	/* Owned strings, released when the client is unmanaged. */
	char		*title;
	char		*instance;
	char		*class_name;
	char		*command;
	unsigned long	pid;
	int		session_claimed;
	XSizeHints	hints;
	struct client	*next;
	struct client	*focus_next;
};

struct wm {
	Display			*display;
	int			screen;
	int			width;
	int			height;
	int			next_order;
	int			current_desktop;
	Window			root;
	Window			taskbar;
	Window			support;
	Atom			atoms[ATOM_COUNT];
	Atom			wm_selection;

	GC			gc;
	struct text_font	*font;
	/* Borrowed pixels from the palette owned by draw.c. */
	const unsigned long	*colors;
	/* Borrowed from the window frame module for other pointer grabs. */
	Cursor			normal_cursor;

	struct client		*clients;
	struct client		*focused;

	Window			desktop_focus[MAX_DESKTOPS];

	struct config		config;
	int			running;
	int			scanning;
	int			restart;
};

extern struct wm wm;

/*
 * wm_property_get returns X11 storage owned by the caller. Release it with
 * XFree.
 */
unsigned char	*wm_property_get(Window, Atom, Atom, int, unsigned long *);
int		 wm_property_has_atom(Window, Atom, Atom);
void		 wm_property_set_cardinals(Window, Atom, const unsigned long *,
	int);

struct client	*window_client_find(Window);
/* Borrowed highest resolved transient parent, or NULL. Traversal is bounded. */
struct client	*window_client_ancestor(const struct client *);
void		 window_client_manage(Window);
void		 window_client_unmanage(struct client *, int);
void		 window_client_refresh(struct client *);
int		 window_client_refresh_property(struct client *, Atom);
void		 window_client_refresh_title(struct client *);

void	window_client_close(struct client *, Time);
int	window_client_is_task(const struct client *);
void	window_client_cleanup(void);

int			 desktop_work_height(void);
void			 desktop_update_workarea(void);
const struct client	*desktop_owner(const struct client *);
int			 desktop_for_client(const struct client *);
int			 desktop_contains(const struct client *);
void			 desktop_init(void);
void			 desktop_publish(void);
void			 desktop_switch(int, Time);
void			 desktop_send(struct client *, int, Time);
int			 desktop_offset(int, int);

void	focus_cycle_add(struct client *);
void	focus_cycle_forget(struct client *);
void	focus_cycle_record(struct client *);
int	focus_cycle_active(void);
void	focus_cycle_begin(unsigned int, Time);
void	focus_cycle_finish(void);
void	focus_cycle_draw(void);
void	focus_cycle_free(void);
void	focus_cycle_step(int, Time);
int	focus_cycle_event(XEvent *);

void	owm_shortcuts_grab(void);
void	owm_shortcuts_press(XKeyEvent *);

void	owm_menu_open(int, int, Time);
void	owm_menu_open_window(struct client *, int, int, Time, int);
void	owm_menu_forget(struct client *);
int	owm_menu_event(XEvent *);
void	owm_menu_close(void);
void	owm_menu_free(void);

void	desktop_icons_init(void);
void	desktop_icons_free(void);
void	desktop_icons_resize(void);
void	desktop_icons_cancel_drag(void);
int	desktop_icons_event(XEvent *);

void	wm_draw_init(void);
void	wm_draw_free(void);
void	wm_draw_destroy_window(Window);
void	wm_draw_box(Window, int, int, int, int, int);
void	wm_draw_surface(Window, int, int, int, int, unsigned long, int);
void	wm_draw_text(Window, int, int, int, int, const char *, unsigned long);
void	wm_draw_title(Window, int, int, int, int, const char *, unsigned long);

int	window_frame_width(const struct client *);
int	window_frame_height(const struct client *);
int	window_frame_top(void);
void	window_frame_init(void);
void	window_frame_free(void);
void	window_frame_draw(struct client *);
void	window_frame_press(struct client *, XButtonEvent *);
void	window_frame_motion(XMotionEvent *);
void	window_frame_release(XButtonEvent *);
int	window_frame_drag_active(void);
int	window_frame_resizing(const struct client *);
/* NULL cancels the current drag regardless of which client owns it. */
void	window_frame_cancel_drag(const struct client *);
void	window_frame_reset_click(void);
/* Forget a departing client's drag and double click state. */
void	window_frame_forget(const struct client *);

int	window_move_begin(struct client *, Time);
int	window_move_active(void);
int	window_move_event(XEvent *);
void	window_move_finish(int);
void	window_move_forget(struct client *);

struct client	*taskbar_client_at(int, int);
void		 taskbar_init(void);
void		 taskbar_free(void);
void		 taskbar_draw(void);
void		 taskbar_draw_tray(void);
void		 taskbar_draw_client(struct client *);
void		 taskbar_focus_changed(struct client *);
void		 taskbar_press(XButtonEvent *);
void		 taskbar_resize(void);
int		 taskbar_pixels(int);
void		 taskbar_icons_init(void);
void		 taskbar_icons_free(void);
void		 taskbar_icon_update(struct client *);
void		 taskbar_icon_free(struct client *);
void		 taskbar_icon_draw(struct client *, Drawable, int, int, int);
int		 taskbar_tooltip_event(XEvent *);
void		 taskbar_tooltip_update(double);
double		 taskbar_tooltip_deadline(void);
void		 taskbar_tooltip_free(void);

/* The caller frees the captured command. NULL means none was available. */
char	*session_command_capture(struct client *);
void	 owm_session_load(void);
int	 owm_session_save(void);
int	 owm_session_match(struct client *);
void	 owm_session_launch(void);
void	 owm_session_tick(double);
double	 owm_session_deadline(void);
void	 owm_session_reaped(pid_t, int);
void	 owm_session_free(void);

#endif
