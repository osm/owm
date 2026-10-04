#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "settings.h"
#include "tray-widgets.h"

struct battery_state {
	int	charging;
	int	have_online;
	int	online;
	int	percentage;
};

static int	read_supply(const char *, const char *, const char *, char *,
	size_t);
static void	read_power_supply(const char *, const char *,
	struct battery_state *);
static void	read_battery_state(struct battery_state *);

static int
read_supply(const char *directory, const char *name, const char *field,
	char *out, size_t size)
{
	FILE *file = NULL;
	int n, ok;
	char path[PATH_MAX];
	char *s = NULL;

	n = snprintf(path, sizeof(path), "%s/%s/%s", directory, name, field);

	if (n < 0 || n >= (int)sizeof(path))
		return 0;

	file = fopen(path, "r");

	if (file == NULL)
		return 0;

	ok = fgets(out, (int)size, file) != NULL;

	fclose(file);

	if (!ok)
		return 0;

	s = trim(out);

	memmove(out, s, strlen(s) + 1);

	return 1;
}

static void
read_power_supply(
	const char *directory, const char *name, struct battery_state *state)
{
	int present, value;
	char capacity[64], status[64] = "", type[64], value_text[64];

	if (name[0] == '.' ||
		!read_supply(directory, name, "type", type, sizeof(type)))
		return;

	if (strcmp(type, "Battery")) {
		if (!read_supply(directory, name, "online", value_text,
			sizeof(value_text)) ||
			!parse_integer(value_text, 0, 1, &value))
			return;

		state->have_online = 1;
		state->online |= value != 0;

		return;
	}

	present = !read_supply(directory, name, "present", value_text,
			sizeof(value_text)) ||
		strcmp(value_text, "0");

	if (state->percentage >= 0 || !present ||
		!read_supply(directory, name, "capacity", capacity,
			sizeof(capacity)) ||
		!parse_integer(capacity, 0, 100, &value))
		return;

	state->percentage = value;
	read_supply(directory, name, "status", status, sizeof(status));
	state->charging = strcmp(status, "Charging") == 0 ||
		strcmp(status, "Full") == 0 ||
		strcmp(status, "Not charging") == 0;
}

static void
read_battery_state(struct battery_state *state)
{
	struct dirent **entries = NULL;
	int count, i;
	const char *directory = NULL;

	state->charging = 0;
	state->have_online = 0;
	state->online = 0;
	state->percentage = -1;

	directory = getenv("OWM_POWER_SUPPLY_PATH");

	if (directory == NULL)
		directory = "/sys/class/power_supply";

	count = scandir(directory, &entries, NULL, alphasort);

	if (count < 0)
		return;

	for (i = 0; i < count; i++) {
		read_power_supply(directory, entries[i]->d_name, state);
		free(entries[i]);
	}

	free(entries);
}

void
tray_battery_update(struct tray_widget *widget, double unused)
{
	struct battery_state state;
	int index, plugged;

	(void)unused;

	widget->text[0] = '\0';
	widget->icon = TRAY_ICON_NONE;

	read_battery_state(&state);

	if (state.percentage < 0)
		return;

	plugged = state.have_online ? state.online : state.charging;
	index = plugged ? TRAY_ICON_POWER : TRAY_ICON_BATTERY;

	if (tray_widget_icon_available(index)) {
		widget->icon = index;
		snprintf(widget->text, sizeof(widget->text), "%d%%",
			state.percentage);
		return;
	}

	snprintf(widget->text, sizeof(widget->text), "BAT %d%%%s",
		state.percentage, plugged ? "+" : "");
}
