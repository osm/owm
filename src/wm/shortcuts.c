#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <err.h>
#include <string.h>

#include "actions.h"
#include "wm.h"

static unsigned int lock_mask = LockMask;

static unsigned int	read_lock_mask(void);

static unsigned int
read_lock_mask(void)
{
	XModifierKeymap *map = NULL;
	int index, modifier;
	unsigned int mask = LockMask;

	map = XGetModifierMapping(wm.display);

	if (map == NULL)
		return mask;

	for (modifier = 0; modifier < 8; modifier++) {
		for (index = 0; index < map->max_keypermod; index++) {
			KeySym symbol;
			KeyCode code;

			code = map->modifiermap[modifier * map->max_keypermod +
				index];

			if (code == 0)
				continue;

			symbol = XkbKeycodeToKeysym(wm.display, code, 0, 0);

			if (symbol == XK_Num_Lock || symbol == XK_Scroll_Lock)
				mask |= 1U << modifier;
		}
	}

	XFreeModifiermap(map);

	return mask;
}

void
owm_shortcuts_grab(void)
{
	int binding_index;

	XUngrabKey(wm.display, AnyKey, AnyModifier, wm.root);
	lock_mask = read_lock_mask();

	for (binding_index = 0; binding_index < wm.config.binding_count;
		binding_index++) {
		struct binding *b = NULL;
		unsigned int extra, locks;
		KeyCode code;

		b = &wm.config.bindings[binding_index];

		b->keycode = XKeysymToKeycode(wm.display, b->symbol);

		if (strcmp(b->action, "none") == 0)
			continue;

		code = b->keycode;

		if (code == 0) {
			warnx("shortcut key %s is unavailable",
				XKeysymToString(b->symbol));
			continue;
		}

		locks = lock_mask & ~b->modifiers;

		for (extra = 0; extra < 256; extra++) {
			if (extra & ~locks)
				continue;

			XGrabKey(wm.display, code, b->modifiers | extra,
				wm.root, True, GrabModeAsync, GrabModeAsync);
		}
	}
}

void
owm_shortcuts_press(XKeyEvent *event)
{
	int i;

	for (i = 0; i < wm.config.binding_count; i++) {
		struct binding *b = NULL;
		unsigned int modifiers;

		b = &wm.config.bindings[i];
		modifiers = event->state & 255U & ~(lock_mask & ~b->modifiers);

		if (modifiers != b->modifiers || event->keycode != b->keycode)
			continue;

		owm_action_execute(
			b->action, b->modifiers & ~lock_mask, event->time);

		return;
	}
}
