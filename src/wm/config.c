#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "paths.h"
#include "settings.h"
#include "string-util.h"
#include "ui-colors.h"
#include "ui-config.h"

static void	config_defaults(struct config *);
static void	config_read(struct config *, FILE *, const char *);
static int	config_option(struct config *, const char *, char *,
	const char **);

static int
config_option(
	struct config *c, const char *key, char *value, const char **expected)
{
	int enabled, ok;

	if (strcmp(key, "taskbar_height") == 0) {
		*expected = "expected 24–64 pixels";
		return parse_integer(value, 24, 64, &c->taskbar_height);
	}

	if (strcmp(key, "taskbar_hide_empty_desktops") == 0) {
		*expected = "expected true or false";
		return parse_boolean(value, &c->taskbar_hide_empty_desktops);
	}

	if (strcmp(key, "taskbar_hide_single_desktop") == 0) {
		*expected = "expected true or false";
		return parse_boolean(value, &c->taskbar_hide_single_desktop);
	}

	if (strcmp(key, "titlebar_height") == 0) {
		*expected = "expected 18–40 pixels";
		return parse_integer(value, 18, 40, &c->titlebar_height);
	}

	if (strcmp(key, "desktops") == 0) {
		*expected = "expected 1–8 desktops";
		return parse_integer(value, 1, MAX_DESKTOPS, &c->desktops);
	}

	if (strcmp(key, "double_click_ms") == 0) {
		*expected = "expected 100–1000 milliseconds";
		return parse_integer(value, 100, 1000, &c->double_click_ms);
	}

	if (strcmp(key, "move_step") == 0) {
		*expected = "expected 1–100 pixels";
		return parse_integer(value, 1, 100, &c->move_step);
	}

	if (strcmp(key, "resize_step") == 0) {
		*expected = "expected 1–100 pixels";
		return parse_integer(value, 1, 100, &c->resize_step);
	}

	if (strcmp(key, "desktop_icon") == 0) {
		char *icon = NULL;

		*expected = "expected \"id | label | command | image\" with a "
			"unique id and an absolute image path, builtin "
			"icon, or "
			"empty image";

		icon = decode_string(value);

		ok = icon != NULL && owm_config_icon(c, icon);
		free(icon);

		return ok;
	}

	if (strcmp(key, "menu_item") == 0) {
		char *item = NULL;

		*expected = "expected \"label | action\" and room in the menu";

		item = decode_string(value);

		ok = item != NULL && owm_config_menu_item(c, item);
		free(item);

		return ok;
	}

	if (strcmp(key, "clear_menu") == 0) {
		*expected = "expected true or false";
		if (!parse_boolean(value, &enabled))
			return 0;

		if (enabled)
			c->menu_count = 0;

		return 1;
	}

	if (strcmp(key, "menu_separator") == 0) {
		*expected = "expected true or false and room in the menu";
		if (!parse_boolean(value, &enabled))
			return 0;

		if (!enabled)
			return 1;

		if (c->menu_count >= MAX_MENU_ITEMS)
			return 0;

		memset(&c->menu[c->menu_count++], 0, sizeof(struct menu_item));

		return 1;
	}

	if (strcmp(key, "bind") == 0) {
		char *binding = NULL;

		*expected = "expected \"Modifier+Key action\" with a valid "
			"key and action";

		binding = decode_string(value);

		ok = binding != NULL && owm_config_binding(c, binding);
		free(binding);

		return ok;
	}

	if (strcmp(key, "clear_bindings") == 0) {
		*expected = "expected true or false";
		if (!parse_boolean(value, &enabled))
			return 0;

		if (enabled)
			owm_config_bindings_free(c);

		return 1;
	}

	if (strcmp(key, "focus_follows_mouse") == 0) {
		*expected = "expected true or false";
		return parse_boolean(value, &c->focus_follows_mouse);
	}

	if (strcmp(key, "tray_battery") == 0) {
		*expected = "expected true or false";
		return parse_boolean(value, &c->tray_battery);
	}

	if (strcmp(key, "tray_wireguard") == 0) {
		*expected = "expected true or false";
		return parse_boolean(value, &c->tray_wireguard);
	}

	if (strcmp(key, "tray_clock") == 0) {
		*expected = "expected true or false";
		return parse_boolean(value, &c->tray_clock);
	}

	if (strcmp(key, "restore_session") == 0) {
		*expected = "expected true or false";
		return parse_boolean(value, &c->restore_session);
	}

	if (strcmp(key, "background_color") == 0) {
		int i;
		char *color = NULL;

		*expected = "expected a quoted hex color, e.g. \"#3e515b\"";

		color = decode_string(value);

		ok = color != NULL && strlen(color) == 7 && color[0] == '#';

		for (i = 1; ok && i < 7; i++)
			ok = isxdigit((unsigned char)color[i]) != 0;

		if (ok)
			string_copy(c->background_color, color,
				sizeof(c->background_color));
		free(color);

		return ok;
	}

	if (strcmp(key, "font") == 0) {
		*expected = "expected a nonempty quoted font name";
		return ui_config_parse_font(c->font, value);
	}

	return 0;
}

static void
config_defaults(struct config *c)
{
	c->taskbar_height = TASKBAR_BASE_HEIGHT;
	c->taskbar_hide_empty_desktops = 1;
	c->taskbar_hide_single_desktop = 1;
	c->titlebar_height = 20;
	c->restore_session = 1;
	c->focus_follows_mouse = 0;
	c->tray_battery = c->tray_wireguard = c->tray_clock = 1;
	c->desktops = MAX_DESKTOPS;
	c->double_click_ms = 300;
	c->move_step = 10;
	c->resize_step = 10;
	owm_config_bindings_defaults(c);
	owm_config_menu_defaults(c);
	string_copy(c->font, DEFAULT_FONT, sizeof(c->font));
	string_copy(c->background_color, ui_color(DESKTOP),
		sizeof(c->background_color));
}

/* Parse the borrowed stream. The caller opens and closes it. */
static void
config_read(struct config *c, FILE *file, const char *path)
{
	struct settings_reader reader = {0};
	int status;

	while ((status = settings_read_line(&reader, file)) > 0) {
		int ok = 0;
		const char *expected = "expected option = value";
		char *key = NULL, *value = NULL;

		key = reader.text;

		if (!settings_split_assignment(reader.text, &key, &value)) {
			warnx("%s:%u: %s: %s", path, reader.line_number, key,
				expected);
			continue;
		}

		expected = "unknown option";
		ok = config_option(c, key, value, &expected);

		if (!ok)
			warnx("%s:%u: %s: %s", path, reader.line_number, key,
				expected);
	}

	if (status < 0)
		warn("%s:%u: cannot read config", path, reader.line_number);
	settings_reader_free(&reader);
}

int
owm_config_load(struct config *c)
{
	FILE *file = NULL;
	size_t n;
	char path[PATH_MAX];

	config_defaults(c);

	if (!config_directory(c->directory, sizeof(c->directory) - 16)) {
		warn("cannot resolve configuration directory");
		return 0;
	}

	n = strlen(c->directory);

	memcpy(c->session, c->directory, n);
	string_copy(c->session + n, "/session", sizeof(c->session) - n);

	memcpy(path, c->directory, n);
	string_copy(path + n, "/config", sizeof(path) - n);

	file = fopen(path, "r");

	if (file == NULL) {
		if (errno != ENOENT)
			warn("%s", path);
		return 1;
	}

	config_read(c, file, path);
	fclose(file);

	return 1;
}

int
owm_config_make_directory(const struct config *c)
{
	if (directory_create(c->directory))
		return 1;

	warn("cannot create directory %s", c->directory);

	return 0;
}
