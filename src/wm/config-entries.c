#include <X11/keysym.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "actions.h"
#include "builtin-icons.h"
#include "config.h"
#include "memory.h"
#include "settings.h"
#include "string-util.h"

static int	parse_chord(char *, KeySym *, unsigned int *);
static int	store_binding(struct config *, KeySym, unsigned int,
	const char *);
static int	valid_label(const char *);

/* Parse in place. The caller owns the chord buffer. */
static int
parse_chord(char *key, KeySym *symbol, unsigned int *modifiers)
{
	char *plus = NULL;

	*modifiers = 0;

	while ((plus = strchr(key, '+')) != NULL) {
		*plus = '\0';

		if (strcmp(key, "Ctrl") == 0 || strcmp(key, "Control") == 0)
			*modifiers |= ControlMask;
		else if (strcmp(key, "Alt") == 0 || strcmp(key, "Mod1") == 0)
			*modifiers |= Mod1Mask;
		else if (strcmp(key, "Super") == 0 || strcmp(key, "Win") == 0 ||
			strcmp(key, "Mod4") == 0)
			*modifiers |= Mod4Mask;
		else if (strcmp(key, "Shift") == 0)
			*modifiers |= ShiftMask;
		else if (strcmp(key, "Mod2") == 0)
			*modifiers |= Mod2Mask;
		else if (strcmp(key, "Mod3") == 0)
			*modifiers |= Mod3Mask;
		else if (strcmp(key, "Mod5") == 0)
			*modifiers |= Mod5Mask;
		else
			return 0;
		key = plus + 1;
	}

	*symbol = XStringToKeysym(key);

	if (*symbol == NoSymbol)
		return 0;

	/* Match the unshifted keysym. Shift is always an explicit modifier. */
	if (*symbol >= XK_A && *symbol <= XK_Z)
		*symbol += XK_a - XK_A;

	return 1;
}

static int
store_binding(struct config *config, KeySym symbol, unsigned int modifiers,
	const char *action)
{
	int i, index;

	index = config->binding_count;

	for (i = 0; i < config->binding_count; i++) {
		if (config->bindings[i].symbol == symbol &&
			config->bindings[i].modifiers == modifiers) {
			index = i;
			break;
		}
	}

	if (index == MAX_BINDINGS)
		return 0;

	if (index == config->binding_count)
		config->binding_count++;
	free(config->bindings[index].action);
	config->bindings[index].symbol = symbol;
	config->bindings[index].modifiers = modifiers;
	config->bindings[index].action = copy_string(action);
	config->bindings[index].keycode = 0;

	return 1;
}

int
owm_config_binding(struct config *config, const char *text)
{
	KeySym symbol;
	int desktop, ok;
	unsigned int modifiers;
	char *action = NULL, *chord = NULL, *input = NULL;

	input = copy_string(text);
	chord = trim(input);
	action = chord;

	while (*action != '\0' && !isspace((unsigned char)*action))
		action++;

	if (*action == '\0') {
		free(input);
		return 0;
	}

	*action++ = '\0';
	action = trim(action);

	ok = owm_action_parse(action, &desktop) != INVALID &&
		parse_chord(chord, &symbol, &modifiers);
	if (ok)
		ok = store_binding(config, symbol, modifiers, action);
	free(input);

	return ok;
}

void
owm_config_bindings_free(struct config *config)
{
	int i;

	for (i = 0; i < config->binding_count; i++) {
		free(config->bindings[i].action);
		config->bindings[i].action = NULL;
	}

	config->binding_count = 0;
}

void
owm_config_bindings_defaults(struct config *config)
{
	size_t i;
	static const char *defaults[] = {
#include "default-bindings.h"
	};

	for (i = 0; i < sizeof(defaults) / sizeof(*defaults); i++)
		owm_config_binding(config, defaults[i]);
}

static int
valid_label(const char *label)
{
	const unsigned char *p = NULL;

	if (*label == '\0')
		return 0;

	for (p = (const unsigned char *)label; *p != '\0'; p++)
		if (*p < 32 || *p == 127)
			return 0;

	return 1;
}

int
owm_config_menu_item(struct config *config, const char *text)
{
	struct menu_item *item = NULL;
	char *action = NULL, *copy = NULL, *divider = NULL, *label = NULL;

	if (config->menu_count == MAX_MENU_ITEMS)
		return 0;

	copy = copy_string(text);
	divider = strchr(copy, '|');

	if (divider == NULL) {
		free(copy);
		return 0;
	}

	*divider++ = '\0';
	label = trim(copy);
	action = trim(divider);

	if (!valid_label(label) ||
		strlen(label) >= sizeof(config->menu[0].label) ||
		strlen(action) >= sizeof(config->menu[0].action) ||
		!owm_action_menu_valid(action)) {
		free(copy);
		return 0;
	}

	item = &config->menu[config->menu_count++];
	string_copy(item->label, label, sizeof(item->label));
	string_copy(item->action, action, sizeof(item->action));
	free(copy);

	return 1;
}

void
owm_config_menu_defaults(struct config *config)
{
	config->menu_count = 0;
	owm_config_menu_item(config, "Run... | exec owm-run");
	memset(&config->menu[config->menu_count++], 0,
		sizeof(struct menu_item));
	owm_config_menu_item(config, "Save | session_save");
	owm_config_menu_item(config, "Restart | session_restart");
	memset(&config->menu[config->menu_count++], 0,
		sizeof(struct menu_item));
	owm_config_menu_item(config, "Exit and save | session_save_exit");
	owm_config_menu_item(config, "Exit without saving | session_exit");
}

int
owm_config_icon(struct config *config, const char *text)
{
	struct desktop_icon *icon = NULL;
	int i, ok;
	char *command = NULL, *copy = NULL, *id = NULL;
	char *image = NULL, *label = NULL;
	const char *p = NULL;

	if (config->icon_count == MAX_DESKTOP_ICONS)
		return 0;

	copy = copy_string(text);
	label = strchr(copy, '|');
	command = (label != NULL) ? strchr(label + 1, '|') : NULL;
	image = (command != NULL) ? strrchr(command + 1, '|') : NULL;
	ok = 0;

	if (label == NULL || command == NULL || image == NULL)
		goto done;
	*label++ = *command++ = *image++ = '\0';

	id = trim(copy);

	label = trim(label);
	command = trim(command);
	image = trim(image);

	icon = &config->icons[config->icon_count];

	if (*id == '\0' || *label == '\0' || *command == '\0' ||
		strlen(id) >= sizeof(icon->id) ||
		strlen(label) >= sizeof(icon->label) ||
		strlen(command) >= sizeof(icon->command) ||
		strlen(image) >= sizeof(icon->image) ||
		(*image != '/' && builtin_icon_parse(image) == BUILTIN_INVALID))
		goto done;

	for (p = id; (*p != '\0'); p++)
		if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
			(*p >= '0' && *p <= '9') || *p == '-' || *p == '_'))
			goto done;

	for (i = 0; i < config->icon_count; i++)
		if (strcmp(config->icons[i].id, id) == 0)
			goto done;

	string_copy(icon->id, id, sizeof(icon->id));
	string_copy(icon->label, label, sizeof(icon->label));
	string_copy(icon->command, command, sizeof(icon->command));
	string_copy(icon->image, image, sizeof(icon->image));
	config->icon_count++;
	ok = 1;
done:
	free(copy);

	return ok;
}
