#ifndef OWM_BUILTIN_ICONS_H
#define OWM_BUILTIN_ICONS_H

#include <X11/Xlib.h>

#include "ui-colors.h"

enum builtin_icon {
	BUILTIN_INVALID = -1,
	BUILTIN_APPLICATION,
	BUILTIN_TERMINAL,
	BUILTIN_BATTERY,
	BUILTIN_NETWORK,
	BUILTIN_POWER
};

enum builtin_icon	builtin_icon_parse(const char *);
void			builtin_icon_draw(Display *, Drawable, GC, int, int,
	int, enum builtin_icon, const unsigned long[COLOR_COUNT]);
/* The caller owns the returned pixmap and releases it with XFreePixmap. */
Pixmap	builtin_icon_pixmap(Display *, Drawable, int, int, enum builtin_icon,
	unsigned long, const unsigned long[COLOR_COUNT]);

#endif
