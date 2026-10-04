#ifndef OWM_CONFIG_H
#define OWM_CONFIG_H

#include <X11/Xlib.h>
#include <limits.h>

#include "constraints.h"
#include "ui-config.h"

enum {
	TASKBAR_BASE_HEIGHT = 28
};

struct binding {
	KeySym		symbol;
	unsigned int	modifiers;
	char		*action;
	KeyCode		keycode;
};

struct menu_item {
	char	label[128];
	char	action[2048];
};

struct desktop_icon {
	char	id[64];
	char	label[128];
	char	command[2048];
	char	image[PATH_MAX];
};

/*
 * Initialize to zero. Binding action strings are owned by this configuration.
 */
struct config {
	int			taskbar_height;
	int			taskbar_hide_empty_desktops;
	int			taskbar_hide_single_desktop;
	int			titlebar_height;
	int			desktops;
	int			double_click_ms;
	int			move_step;
	int			resize_step;
	int			restore_session;
	int			focus_follows_mouse;
	int			tray_battery;
	int			tray_wireguard;
	int			tray_clock;
	char			background_color[8];
	char			font[FONT_NAME_SIZE];
	char			directory[PATH_MAX];
	char			session[PATH_MAX];

	struct binding		bindings[MAX_BINDINGS];
	int			binding_count;

	struct menu_item	menu[MAX_MENU_ITEMS];
	int			menu_count;

	struct desktop_icon	icons[MAX_DESKTOP_ICONS];
	int			icon_count;
};

/* Load defaults and file options without requiring a running WM. */
int	owm_config_load(struct config *);
int	owm_config_make_directory(const struct config *);
void	owm_config_bindings_defaults(struct config *);
int	owm_config_binding(struct config *, const char *);
/* Release binding strings before discarding the configuration. */
void	owm_config_bindings_free(struct config *);
void	owm_config_menu_defaults(struct config *);
int	owm_config_menu_item(struct config *, const char *);
int	owm_config_icon(struct config *, const char *);

#endif
