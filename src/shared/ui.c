#include <err.h>
#include <stdio.h>
#include <string.h>

#include "ui.h"

int
ui_init(struct ui *ui, Display *display, Window window, const char *font_name)
{
	memset(ui, 0, sizeof(*ui));
	ui->display = display;
	ui->font = font_open(display, font_name);

	if (ui->font == NULL) {
		warnx("no usable font");
		return 0;
	}

	ui->gc = XCreateGC(display, window, 0, NULL);
	ui_palette_load(&ui->palette, display, DefaultScreen(display), NULL);
	ui->colors = ui->palette.pixels;
	ui->owns_resources = 1;

	return 1;
}

void
ui_borrow(struct ui *ui, Display *display, GC gc, struct text_font *font,
	const unsigned long *colors)
{
	memset(ui, 0, sizeof(*ui));
	ui->display = display;
	ui->gc = gc;
	ui->font = font;
	ui->colors = colors;
}

void
ui_free(struct ui *ui)
{
	if (ui->owns_resources) {
		font_close(ui->font);
		XFreeGC(ui->display, ui->gc);
		ui_palette_free(&ui->palette);
	}
	ui->colors = NULL;
	ui->font = NULL;
	ui->gc = NULL;
	ui->owns_resources = 0;
	ui->clipped = 0;
}

int
ui_contains(struct ui_rect rect, int x, int y)
{
	return x >= rect.x && x < rect.x + rect.width && y >= rect.y &&
		y < rect.y + rect.height;
}

void
ui_rect(struct canvas canvas, int x, int y, int width, int height,
	unsigned long pixel)
{
	XSetForeground(canvas.ui->display, canvas.ui->gc, pixel);
	XFillRectangle(canvas.ui->display, canvas.window, canvas.ui->gc, x, y,
		width, height);
}

void
ui_label(struct canvas canvas, int x, int y, const char *text,
	unsigned long pixel)
{
	XSetForeground(canvas.ui->display, canvas.ui->gc, pixel);
	font_draw(canvas.ui->font, canvas.window, x, y, text, (int)strlen(text),
		pixel, canvas.ui->clipped ? &canvas.ui->clip : NULL);
}

void
ui_bevel(struct canvas canvas, int x, int y, int width, int height, int inset)
{
	unsigned long bottom, top;
	int i;

	top = inset ? canvas.ui->colors[SHADOW] : canvas.ui->colors[LIGHT];
	bottom = inset ? canvas.ui->colors[LIGHT] : canvas.ui->colors[SHADOW];

	for (i = 0; i < 2; i++) {
		ui_rect(canvas, x + i, y + i, width - 2 * i, 1, top);
		ui_rect(canvas, x + i, y + i, 1, height - 2 * i, top);
		ui_rect(canvas, x + i, y + height - 1 - i, width - 2 * i, 1,
			bottom);
		ui_rect(canvas, x + width - 1 - i, y + i, 1, height - 2 * i,
			bottom);
	}
}

void
ui_button(struct canvas canvas, struct ui_rect bounds, const char *text,
	int active, int enabled)
{
	int baseline, text_x, width, x, y;
	char dash;

	x = bounds.x;
	y = bounds.y;
	width = bounds.width;

	ui_rect(canvas, x, y, width, bounds.height, canvas.ui->colors[FACE]);
	ui_bevel(canvas, x, y, width, bounds.height, 0);

	text_x = x +
		(width - font_width(canvas.ui->font, text, (int)strlen(text))) /
			2;
	baseline = y +
		(bounds.height - canvas.ui->font->ascent -
			canvas.ui->font->descent) /
			2 +
		canvas.ui->font->ascent;

	if (!enabled)
		ui_label(canvas, text_x + 1, baseline + 1, text,
			canvas.ui->colors[WHITE]);
	ui_label(canvas, text_x, baseline, text,
		enabled ? canvas.ui->colors[BLACK] : canvas.ui->colors[SHADOW]);

	if (!active)
		return;

	XSetForeground(
		canvas.ui->display, canvas.ui->gc, canvas.ui->colors[BLACK]);
	XSetLineAttributes(canvas.ui->display, canvas.ui->gc, 0, LineOnOffDash,
		CapButt, JoinMiter);

	dash = 1;

	XSetDashes(canvas.ui->display, canvas.ui->gc, 0, &dash, 1);
	XDrawRectangle(canvas.ui->display, canvas.window, canvas.ui->gc, x + 4,
		y + 4, width - 9, bounds.height - 9);
	XSetLineAttributes(canvas.ui->display, canvas.ui->gc, 0, LineSolid,
		CapButt, JoinMiter);
}

void
ui_clip(struct ui *ui, const XRectangle *clip)
{
	ui->clipped = clip != NULL;

	if (clip != NULL) {
		ui->clip = *clip;
		XSetClipRectangles(
			ui->display, ui->gc, 0, 0, &ui->clip, 1, Unsorted);
		return;
	}

	XSetClipMask(ui->display, ui->gc, None);
}
