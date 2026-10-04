/* Configuration and action parsing link without the WM runtime. */
#include <sys/stat.h>

#include <X11/keysym.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/shared/builtin-icons.h"
#include "../src/shared/settings.h"
#include "../src/wm/actions.h"
#include "../src/wm/config.h"

static void	check_default_bindings(const struct config *);
static void	test_config_file(void);
static void	test_actions(void);
static void	test_icon_names(void);

static void
check_default_bindings(const struct config *defaults)
{
	struct config shipped = {0};
	struct settings_reader reader = {0};
	FILE *file = NULL;
	int i, desktop, status;

	file = fopen("config", "r");
	assert(file != NULL);
	while ((status = settings_read_line(&reader, file)) > 0) {
		char *key = NULL, *value = NULL, *binding = NULL;

		assert(settings_split_assignment(reader.text, &key, &value));
		if (strcmp(key, "bind"))
			continue;
		binding = decode_string(value);
		assert(binding != NULL);
		assert(owm_config_binding(&shipped, binding));
		free(binding);
	}
	assert(status == 0);
	settings_reader_free(&reader);
	assert(fclose(file) == 0);
	assert(defaults->binding_count == shipped.binding_count);
	for (i = 0; i < shipped.binding_count; i++) {
		enum action action;

		assert(defaults->bindings[i].symbol ==
			shipped.bindings[i].symbol);
		assert(defaults->bindings[i].modifiers ==
			shipped.bindings[i].modifiers);
		assert(!strcmp(defaults->bindings[i].action,
			shipped.bindings[i].action));
		action = owm_action_parse(shipped.bindings[i].action, &desktop);
		assert(action > INVALID && action < UNBIND);
	}
	owm_config_bindings_free(&shipped);
}

static void
test_config_file(void)
{
	struct config config = {0};
	FILE *file = NULL;
	char directory[] = "/tmp/owm-config-test.XXXXXX";
	char path[PATH_MAX];

	assert(mkdtemp(directory) != NULL);
	assert(setenv("XDG_CONFIG_HOME", directory, 1) == 0);
	assert(unsetenv("DISPLAY") == 0);
	snprintf(path, sizeof(path), "%s/owm", directory);
	assert(mkdir(path, 0700) == 0);
	snprintf(path, sizeof(path), "%s/owm/config", directory);
	assert(owm_config_load(&config));
	check_default_bindings(&config);
	assert(config.taskbar_hide_empty_desktops);
	assert(config.taskbar_hide_single_desktop);
	assert(config.desktops == 8);
	owm_config_bindings_free(&config);
	memset(&config, 0, sizeof(config));
	file = fopen(path, "w");
	assert(file != NULL);
	fputs("clear_bindings = true\n"
	"bind = \"Ctrl+A desktop_switch 2\"\n"
	"bind = \"Ctrl+a window_send_to_desktop 3\"\n"
	"clear_menu = true\n"
	"menu_item = \"Terminal | exec printf hello | cat\"\n"
	"desktop_icon = \"terminal | Terminal | xterm | \"\n"
	"font = \"fixed\"\n"
	"desktops = 3\n"
	"taskbar_hide_empty_desktops = false\n"
	"taskbar_hide_single_desktop = false\n",
		file);
	assert(fclose(file) == 0);

	assert(owm_config_load(&config));
	assert(config.taskbar_height == TASKBAR_BASE_HEIGHT);
	assert(config.desktops == 3 && !strcmp(config.font, "fixed"));
	assert(!config.taskbar_hide_empty_desktops);
	assert(!config.taskbar_hide_single_desktop);
	assert(config.binding_count == 1);
	assert(config.bindings[0].symbol == XK_a);
	assert(config.bindings[0].modifiers == ControlMask);
	assert(!strcmp(config.bindings[0].action, "window_send_to_desktop 3"));
	assert(!owm_config_binding(&config, "Ctrl+a desktop_switch 0"));
	assert(!owm_config_binding(&config, "Unknown+a window_close"));
	assert(config.binding_count == 1);
	assert(!strcmp(config.bindings[0].action, "window_send_to_desktop 3"));

	assert(config.menu_count == 1);
	assert(!strcmp(config.menu[0].action, "exec printf hello | cat"));
	assert(!owm_config_menu_item(&config, "Close | window_close"));
	assert(config.menu_count == 1);
	assert(config.icon_count == 1);
	assert(!owm_config_icon(&config, "terminal | Duplicate | xterm | "));
	assert(config.icon_count == 1);
	assert(!strcmp(config.icons[0].label, "Terminal"));
	assert(owm_config_icon(&config, "server | Server | st | terminal"));
	assert(!strcmp(config.icons[1].image, "terminal"));
	assert(!owm_config_icon(&config, "bad | Bad | true | unknown"));

	owm_config_bindings_free(&config);
	assert(config.binding_count == 0 && config.bindings[0].action == NULL);
	owm_config_bindings_free(&config);
	assert(unlink(path) == 0);
	assert(rmdir(config.directory) == 0);
	assert(rmdir(directory) == 0);
}

static void
test_actions(void)
{
	int desktop;
	char action[64];

	snprintf(action, sizeof(action), "desktop_switch %d", MAX_DESKTOPS);
	assert(owm_action_parse(action, &desktop) == DESKTOP_SWITCH);
	assert(desktop == MAX_DESKTOPS);
	snprintf(action, sizeof(action), "desktop_switch %d", MAX_DESKTOPS + 1);
	assert(owm_action_parse(action, &desktop) == INVALID);
	assert(owm_action_parse("exec \t", &desktop) == INVALID);
	assert(owm_action_parse("exec printf hello | cat", &desktop) == EXEC);
}

static void
test_icon_names(void)
{
	struct config config = {0};
	size_t i;
	char entry[128];

	static const struct {
		const char		*name;
		enum builtin_icon	icon;
	} icons[] = {{"application", BUILTIN_APPLICATION},
		{"terminal", BUILTIN_TERMINAL}, {"battery", BUILTIN_BATTERY},
		{"network", BUILTIN_NETWORK}, {"power", BUILTIN_POWER}};

	for (i = 0; i < sizeof(icons) / sizeof(*icons); i++) {
		assert(builtin_icon_parse(icons[i].name) == icons[i].icon);
		snprintf(entry, sizeof(entry), "%s | Example | true | %s",
			icons[i].name, icons[i].name);
		assert(owm_config_icon(&config, entry));
	}
	assert(builtin_icon_parse("") == BUILTIN_APPLICATION);
	assert(builtin_icon_parse("terminal-extra") == BUILTIN_INVALID);
	assert(!owm_config_icon(&config, "bad | Bad | true | terminal-extra"));
}

int
main(void)
{
	test_config_file();
	test_actions();
	test_icon_names();
	puts("PASS config");

	return 0;
}
