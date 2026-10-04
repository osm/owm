/* Check allocation ownership with a deterministic X11 allocator. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/shared/ui-palette.h"

static unsigned long allocated[COLOR_COUNT];
static unsigned int failures;
static int attempts, outstanding, releases;
static const char *desktop = NULL;

Colormap
XDefaultColormap(Display *display, int screen)
{
	assert(display == NULL && screen == 2);

	return 42;
}

unsigned long
XBlackPixel(Display *display, int screen)
{
	assert(display == NULL && screen == 2);

	return 99;
}

Status
XAllocNamedColor(Display *display, Colormap colormap, const char *name,
	XColor *value, XColor *exact)
{
	int color;

	assert(display == NULL && colormap == 42);
	color = attempts++ % COLOR_COUNT;
	assert(strcmp(name,
		color == DESKTOP && desktop != NULL
			? desktop
			: ui_color(color)) == 0);
	if (failures & (1U << color))
		return 0;

	/*
	 * Separate allocations can share the same pixel, and each needs a free.
	 */
	memset(value, 0, sizeof(*value));
	value->pixel = 100 + color / 2;
	*exact = *value;
	assert(outstanding < COLOR_COUNT);
	allocated[outstanding++] = value->pixel;

	return 1;
}

int
XFreeColors(Display *display, Colormap colormap, unsigned long *pixels,
	int count, unsigned long planes)
{
	int i;

	assert(display == NULL && colormap == 42 && planes == 0);
	assert(count > 0 && count == outstanding);
	for (i = 0; i < count; i++)
		assert(pixels[i] == allocated[i] && pixels[i] != 99);
	outstanding = 0;
	releases++;

	return 1;
}

int
main(void)
{
	struct ui_palette palette = {0};
	int i;

	ui_palette_free(&palette);
	assert(releases == 0);
	failures = 1U << BLACK;
	ui_palette_load(&palette, NULL, 2, NULL);
	assert(attempts == COLOR_COUNT && outstanding == COLOR_COUNT - 1);
	assert(palette.pixels[BLACK] == 99);
	assert(palette.pixels[DESKTOP] == 100);

	desktop = "#123456";
	ui_palette_load(&palette, NULL, 2, desktop);
	assert(releases == 1 && outstanding == COLOR_COUNT - 1);
	ui_palette_free(&palette);
	assert(releases == 2 && outstanding == 0);
	ui_palette_free(&palette);
	assert(releases == 2);

	failures = (1U << COLOR_COUNT) - 1;
	ui_palette_load(&palette, NULL, 2, desktop);
	for (i = 0; i < COLOR_COUNT; i++)
		assert(palette.pixels[i] == 99);
	ui_palette_free(&palette);
	assert(releases == 2 && outstanding == 0);
	puts("PASS ui-palette");

	return 0;
}
