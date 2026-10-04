#ifndef OWM_UI_PALETTE_H
#define OWM_UI_PALETTE_H

#include <X11/Xlib.h>

#include "ui-colors.h"

/* Initialize to zero. Owns allocated colors. The display is borrowed. */
struct ui_palette {
	Display		*display;
	Colormap	colormap;
	unsigned long	pixels[COLOR_COUNT];
	unsigned long	allocated[COLOR_COUNT];
	int		allocated_count;
};

/* Replace the palette. A NULL desktop color selects the default. */
void	ui_palette_load(struct ui_palette *, Display *, int, const char *);
/* Free before closing the display. Fallback pixels are never released. */
void	ui_palette_free(struct ui_palette *);

#endif
