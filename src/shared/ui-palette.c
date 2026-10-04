#include <string.h>

#include "ui-palette.h"

void
ui_palette_load(struct ui_palette *palette, Display *display, int screen,
	const char *desktop)
{
	int i;

	ui_palette_free(palette);
	palette->display = display;
	palette->colormap = XDefaultColormap(display, screen);
	for (i = 0; i < COLOR_COUNT; i++) {
		XColor exact, value;
		const char *name = NULL;

		name = i == DESKTOP && desktop != NULL ? desktop : ui_color(i);
		if (XAllocNamedColor(
			display, palette->colormap, name, &value, &exact)) {
			palette->pixels[i] = value.pixel;
			palette->allocated[palette->allocated_count++] =
				value.pixel;
		} else
			palette->pixels[i] = XBlackPixel(display, screen);
	}
}

void
ui_palette_free(struct ui_palette *palette)
{
	if (palette->allocated_count)
		XFreeColors(palette->display, palette->colormap,
			palette->allocated, palette->allocated_count, 0);
	memset(palette, 0, sizeof(*palette));
}
