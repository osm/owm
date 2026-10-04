#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "ui-palette.h"
#include "ui.h"
#include "utf8.h"
#include "wm.h"

#define LABEL_SIZE 1024

static struct ui_palette palette;

static size_t	fit_label(const char *, char *, int);
static void	draw_label(Window, int, int, int, int, const char *,
	unsigned long, int);

void
wm_draw_init(void)
{
	int height;

	wm.font = font_open(wm.display, wm.config.font);

	if (wm.font == NULL)
		errx(1, "no usable font");

	ui_palette_load(
		&palette, wm.display, wm.screen, wm.config.background_color);
	wm.colors = palette.pixels;

	height = wm.font->ascent + wm.font->descent;

	if (wm.config.titlebar_height < height + 4)
		wm.config.titlebar_height = height + 4;

	if (wm.config.taskbar_height < height + 10)
		wm.config.taskbar_height = height + 10;
	wm.gc = XCreateGC(wm.display, wm.root, 0, NULL);
	XSetWindowBackground(wm.display, wm.root, wm.colors[DESKTOP]);
	XClearWindow(wm.display, wm.root);
}

void
wm_draw_free(void)
{
	font_close(wm.font);
	XFreeGC(wm.display, wm.gc);
	ui_palette_free(&palette);
	wm.colors = NULL;
}

void
wm_draw_destroy_window(Window window)
{
	font_forget_drawable(wm.font, window);
	XDestroyWindow(wm.display, window);
}

void
wm_draw_surface(Window w, int x, int y, int width, int height,
	unsigned long face, int sunken)
{
	struct canvas canvas;
	struct ui ui;

	if (width < 4 || height < 4)
		return;

	ui_borrow(&ui, wm.display, wm.gc, wm.font, wm.colors);
	canvas.ui = &ui;
	canvas.window = w;

	ui_rect(canvas, x, y, width, height, face);
	ui_bevel(canvas, x, y, width, height, sunken);
}

void
wm_draw_box(Window w, int x, int y, int width, int height, int sunken)
{
	wm_draw_surface(w, x, y, width, height, wm.colors[FACE], sunken);
}

/* The caller must provide at least LABEL_SIZE bytes. */
static size_t
fit_label(const char *text, char *buffer, int width)
{
	size_t n;
	int dots, truncated;

	n = strlen((text != NULL) ? text : "");

	if (n > LABEL_SIZE - 4)
		n = utf8_floor(text, LABEL_SIZE - 4);

	memcpy(buffer, (text != NULL) ? text : "", n);
	buffer[n] = '\0';

	truncated = font_width(wm.font, buffer, (int)n) > width;

	if (truncated) {
		size_t boundaries[LABEL_SIZE], count = 0;
		size_t high, i, low;

		dots = font_width(wm.font, "...", 3);
		boundaries[count++] = 0;

		for (i = 0; i < n;) {
			i = utf8_next(buffer, i);
			boundaries[count++] = i;
		}

		low = 0;
		high = count - 1;

		while (low < high) {
			size_t mid;

			mid = low + (high - low + 1) / 2;

			if (font_width(wm.font, buffer, (int)boundaries[mid]) +
					dots <=
				width)
				low = mid;
			else
				high = mid - 1;
		}

		n = boundaries[low];
		memcpy(buffer + n, "...", 3);
		n += 3;
	}

	buffer[n] = '\0';

	return n;
}

static void
draw_label(Window w, int x, int y, int width, int height, const char *text,
	unsigned long pixel, int center_capitals)
{
	size_t n;
	XRectangle clip = {0};
	int baseline;
	char buffer[LABEL_SIZE] = {0};

	if (width <= 0 || height <= 0)
		return;

	clip.x = (short)x;
	clip.y = (short)y;
	clip.width = (unsigned short)width;
	clip.height = (unsigned short)height;

	n = fit_label(text, buffer, width);

	baseline = y + (height - wm.font->ascent - wm.font->descent) / 2 +
		wm.font->ascent;

	if (center_capitals) {
		XGlyphInfo capitals, ink;

		XftTextExtentsUtf8(wm.display, wm.font->handle,
			(const FcChar8 *)buffer, (int)n, &ink);

		XftTextExtentsUtf8(wm.display, wm.font->handle,
			(const FcChar8 *)"H", 1, &capitals);

		/*
		 * Center the main letter body. Descenders should hang below the
		 * baseline rather than lifting the whole title as its text
		 * changes. Keep accents and descenders inside the bevel.
		 */
		if (capitals.height)
			baseline = y + (height - capitals.height + 1) / 2 +
				capitals.y;

		if (ink.height) {
			int highest, lowest;

			lowest = y + height - 2 - ink.height + ink.y;
			highest = y + 2 + ink.y;

			if (baseline > lowest)
				baseline = lowest;
			if (baseline < highest)
				baseline = highest;
		}
	}

	font_draw(wm.font, w, x, baseline, buffer, (int)n, pixel, &clip);
}

void
wm_draw_text(Window w, int x, int y, int width, int height, const char *text,
	unsigned long pixel)
{
	draw_label(w, x, y, width, height, text, pixel, 0);
}

void
wm_draw_title(Window window, int x, int y, int width, int height,
	const char *text, unsigned long pixel)
{
	draw_label(window, x, y, width, height, text, pixel, 1);
}
