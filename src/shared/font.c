#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "font.h"
#include "memory.h"
#include "ui-config.h"

struct text_font *
font_open(Display *display, const char *name)
{
	struct text_font *font = NULL;
	XftFont *handle = NULL;

	handle = XftFontOpenName(display, DefaultScreen(display), name);

	if (handle == NULL) {
		warnx("cannot load font '%s'; using %s", name, DEFAULT_FONT);
		handle = XftFontOpenName(
			display, DefaultScreen(display), DEFAULT_FONT);
	}

	if (handle == NULL)
		return NULL;

	font = allocate(sizeof(*font));

	font->display = display;
	font->handle = handle;
	font->ascent = handle->ascent;
	font->descent = handle->descent;

	return font;
}

void
font_close(struct text_font *font)
{
	if (font == NULL)
		return;

	if (font->draw != NULL)
		XftDrawDestroy(font->draw);
	XftFontClose(font->display, font->handle);
	free(font);
}

void
font_forget_drawable(struct text_font *font, Drawable drawable)
{
	if (font->draw != NULL && XftDrawDrawable(font->draw) == drawable) {
		XftDrawDestroy(font->draw);
		font->draw = NULL;
	}
}

int
font_width(struct text_font *font, const char *text, int length)
{
	XGlyphInfo extent;

	XftTextExtentsUtf8(font->display, font->handle, (const FcChar8 *)text,
		length, &extent);

	return extent.xOff;
}

void
font_draw(struct text_font *font, Window window, int x, int baseline,
	const char *text, int length, unsigned long pixel,
	const XRectangle *clip)
{
	Colormap map;
	Display *display = NULL;
	XftDraw *draw = NULL;
	int screen;
	unsigned int index;

	display = font->display;
	screen = DefaultScreen(display);
	map = DefaultColormap(display, screen);
	if (font->draw == NULL)
		font->draw = XftDrawCreate(
			display, window, DefaultVisual(display, screen), map);
	else if (XftDrawDrawable(font->draw) != window)
		XftDrawChange(font->draw, window);
	draw = font->draw;

	if (draw == NULL)
		return;

	for (index = 0; index < font->color_count; index++)
		if (font->colors[index].pixel == pixel)
			break;

	if (index == font->color_count) {
		XColor color = {0};

		if (font->color_count < 8)
			font->color_count++;
		else
			index = font->next_color++ % 8;

		color.pixel = pixel;

		XQueryColor(display, map, &color);
		font->colors[index].pixel = pixel;
		font->colors[index].color.red = color.red;
		font->colors[index].color.green = color.green;
		font->colors[index].color.blue = color.blue;
		font->colors[index].color.alpha = 65535;
	}

	if (clip != NULL)
		XftDrawSetClipRectangles(draw, 0, 0, clip, 1);
	else
		XftDrawSetClip(draw, NULL);
	XftDrawStringUtf8(draw, &font->colors[index], font->handle, x, baseline,
		(const FcChar8 *)text, length);
}
