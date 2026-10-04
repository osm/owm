#include <string.h>

#include "builtin-icons.h"

struct drawing {
	Display			*display;
	Drawable		target;
	GC			gc;
	int			x;
	int			y;
	int			size;
	const unsigned long	*colors;
};

static void	rect(struct drawing *, int, int, int, int, int);
static void	bevel(struct drawing *, int, int, int, int);
static void	draw_application(struct drawing *);
static void	draw_terminal(struct drawing *);
static void	draw_network(struct drawing *);
static void	draw_battery(struct drawing *);
static void	draw_power(struct drawing *);

enum builtin_icon
builtin_icon_parse(const char *name)
{
	if (*name == '\0' || strcmp(name, "application") == 0)
		return BUILTIN_APPLICATION;
	else if (strcmp(name, "terminal") == 0)
		return BUILTIN_TERMINAL;
	else if (strcmp(name, "battery") == 0)
		return BUILTIN_BATTERY;
	else if (strcmp(name, "network") == 0)
		return BUILTIN_NETWORK;
	else if (strcmp(name, "power") == 0)
		return BUILTIN_POWER;
	else
		return BUILTIN_INVALID;
}

/*
 * Draw on a grid of 32 units, keeping edges sharp at both tray and desktop
 * sizes.
 */
static void
rect(struct drawing *d, int x, int y, int w, int h, int color)
{
	int height, left, top, width;

	left = x * d->size / 32;
	top = y * d->size / 32;
	width = (x + w) * d->size / 32 - left;
	height = (y + h) * d->size / 32 - top;

	if (width < 1)
		width = 1;

	if (height < 1)
		height = 1;
	XSetForeground(d->display, d->gc, d->colors[color]);
	XFillRectangle(d->display, d->target, d->gc, d->x + left, d->y + top,
		(unsigned int)width, (unsigned int)height);
}

static void
bevel(struct drawing *d, int x, int y, int w, int h)
{
	rect(d, x, y, w, h, SHADOW);
	rect(d, x, y, w - 1, 1, WHITE);
	rect(d, x, y, 1, h - 1, LIGHT);
	rect(d, x + 1, y + 1, w - 2, h - 2, FACE);
}

static void
draw_application(struct drawing *d)
{
	bevel(d, 6, 2, 21, 28);
	rect(d, 9, 5, 15, 22, WHITE);
	rect(d, 11, 8, 11, 3, ACTIVE);
	rect(d, 11, 14, 3, 3, SHADOW);
	rect(d, 17, 14, 5, 2, SHADOW);
	rect(d, 11, 20, 3, 3, SHADOW);
	rect(d, 17, 20, 5, 2, SHADOW);
}

static void
draw_terminal(struct drawing *d)
{
	bevel(d, 11, 23, 10, 5);
	bevel(d, 5, 28, 22, 3);
	bevel(d, 1, 3, 30, 22);
	rect(d, 4, 6, 24, 15, BLACK);
	rect(d, 6, 8, 2, 2, WHITE);
	rect(d, 8, 10, 2, 2, WHITE);
	rect(d, 10, 12, 2, 2, WHITE);
	rect(d, 8, 14, 2, 2, WHITE);
	rect(d, 6, 16, 2, 2, WHITE);
	rect(d, 15, 16, 8, 2, LIGHT);
	rect(d, 24, 22, 3, 1, ACTIVE);
}

static void
draw_network(struct drawing *d)
{
	int i;

	rect(d, 7, 18, 2, 8, SHADOW);
	rect(d, 23, 18, 2, 8, SHADOW);
	rect(d, 3, 25, 26, 2, SHADOW);
	rect(d, 3, 27, 26, 1, WHITE);

	for (i = 0; i < 2; i++) {
		int left;

		left = 1 + i * 16;

		bevel(d, left, 4, 14, 14);
		rect(d, left + 2, 6, 10, 8, BLACK);
		rect(d, left + 4, 8, 6, 4, ACTIVE);
		bevel(d, left + 4, 18, 6, 3);
	}
}

static void
draw_battery(struct drawing *d)
{
	int i;

	rect(d, 28, 11, 3, 10, SHADOW);
	bevel(d, 1, 7, 27, 18);
	rect(d, 4, 10, 21, 12, BLACK);

	for (i = 0; i < 3; i++)
		rect(d, 6 + i * 6, 12, 4, 8, ACTIVE);
}

static void
draw_power(struct drawing *d)
{
	/* Two prongs, a solid plug body, and a short cable. */
	rect(d, 9, 2, 4, 9, BLACK);
	rect(d, 19, 2, 4, 9, BLACK);
	bevel(d, 5, 10, 22, 12);
	rect(d, 9, 21, 14, 4, SHADOW);
	rect(d, 14, 24, 4, 7, BLACK);
}

void
builtin_icon_draw(Display *display, Drawable target, GC gc, int x, int y,
	int size, enum builtin_icon icon,
	const unsigned long colors[COLOR_COUNT])
{
	struct drawing d = {0};

	d.display = display;
	d.target = target;
	d.gc = gc;
	d.x = x;
	d.y = y;
	d.size = size;
	d.colors = colors;

	switch (icon) {
	case BUILTIN_TERMINAL:
		draw_terminal(&d);
		break;
	case BUILTIN_NETWORK:
		draw_network(&d);
		break;
	case BUILTIN_BATTERY:
		draw_battery(&d);
		break;
	case BUILTIN_POWER:
		draw_power(&d);
		break;
	case BUILTIN_APPLICATION:
	default:
		draw_application(&d);
		break;
	}
}

Pixmap
builtin_icon_pixmap(Display *display, Drawable target, int depth, int size,
	enum builtin_icon icon, unsigned long background,
	const unsigned long colors[COLOR_COUNT])
{
	GC gc = NULL;
	Pixmap pixmap;

	pixmap = XCreatePixmap(display, target, size, size, depth);
	gc = XCreateGC(display, pixmap, 0, NULL);

	XSetForeground(display, gc, background);
	XFillRectangle(display, pixmap, gc, 0, 0, size, size);
	builtin_icon_draw(display, pixmap, gc, 0, 0, size, icon, colors);
	XFreeGC(display, gc);

	return pixmap;
}
