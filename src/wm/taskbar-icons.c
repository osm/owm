#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "builtin-icons.h"
#include "memory.h"
#include "string-util.h"
#include "wm.h"

/* Bound property reads controlled by clients to 4 MiB on the wire. */
#define ICON_WORDS (1024UL * 1024)
#define MAX_ICON_DIMENSION 1024

struct icon_image {
	const unsigned long	*pixels;
	unsigned long		width, height;
};

static Pixmap generic_icons[2];

/*
 * Cache both button backgrounds when artwork changes, never during focus
 * changes or drawing. Transparent pixels then match the selected button.
 */
static Pixmap		 cache_rgba(struct client *, const unsigned char *,
	unsigned int, unsigned int);
static int		 select_icon(const unsigned long *, unsigned long,
	unsigned long, struct icon_image *);
static unsigned char	*decode_icon(const struct icon_image *);
static Pixmap		 net_icon(struct client *);
static unsigned int	 channel(unsigned long, unsigned long);
static Pixmap		 legacy_icon(struct client *);
static int		 matches(const struct client *, const char *);
static Pixmap		 configured_icon(struct client *);

static Pixmap
cache_rgba(struct client *c, const unsigned char *rgba, unsigned int width,
	unsigned int height)
{
	Pixmap normal;

	normal = wm_image_from_rgba(rgba, width, height, wm.colors[FACE],
		taskbar_pixels(TASKBAR_ICON_SIZE), &c->taskbar_icon_width,
		&c->taskbar_icon_height, 1);

	if (normal)
		c->taskbar_icon_active = wm_image_from_rgba(rgba, width, height,
			wm.colors[LIGHT], taskbar_pixels(TASKBAR_ICON_SIZE),
			&c->taskbar_icon_width, &c->taskbar_icon_height, 1);

	return normal;
}

static int
select_icon(const unsigned long *words, unsigned long count, unsigned long size,
	struct icon_image *image)
{
	unsigned long best_score, i;

	image->pixels = NULL;
	best_score = ~0UL;

	for (i = 0; i < count;) {
		unsigned long height, score, side, width;

		if (count - i < 2)
			return 0;

		width = words[i++];
		height = words[i++];

		if (width == 0 || height == 0 || width > MAX_ICON_DIMENSION ||
			height > MAX_ICON_DIMENSION ||
			width > (count - i) / height)
			return 0;

		/*
		 * Upscaling blurs small icons, so prefer a larger source when
		 * no exact size is available.
		 */
		side = width > height ? width : height;
		score = side >= size ? side - size
				: 2 * MAX_ICON_DIMENSION + size - side;

		if (score < best_score) {
			image->pixels = words + i;
			image->width = width;
			image->height = height;
			best_score = score;
		}

		i += width * height;
	}

	return image->pixels != NULL;
}

static unsigned char *
decode_icon(const struct icon_image *image)
{
	unsigned long i, pixel;
	unsigned char *rgba = NULL;

	rgba = allocate_array(image->height, (size_t)image->width * 4);

	for (i = 0; i < image->width * image->height; i++) {
		pixel = image->pixels[i];
		rgba[i * 4] = pixel >> 16;
		rgba[i * 4 + 1] = pixel >> 8;
		rgba[i * 4 + 2] = pixel;
		rgba[i * 4 + 3] = pixel >> 24;
	}

	return rgba;
}

static Pixmap
net_icon(struct client *c)
{
	struct icon_image image = {0};
	Atom type;
	Pixmap result;
	unsigned long count, remaining;
	int format;
	unsigned char *data = NULL, *rgba = NULL;

	if (XGetWindowProperty(wm.display, c->window, wm.atoms[A_ICON], 0,
		ICON_WORDS, False, XA_CARDINAL, &type, &format, &count,
		&remaining, &data) != Success ||
		type != XA_CARDINAL || format != 32 || remaining) {
		if (data != NULL)
			XFree(data);
		return None;
	}

	if (!select_icon((unsigned long *)data, count,
		(unsigned long)taskbar_pixels(TASKBAR_ICON_SIZE), &image)) {
		if (data != NULL)
			XFree(data);
		return None;
	}

	rgba = decode_icon(&image);
	result = cache_rgba(c, rgba, image.width, image.height);
	free(rgba);
	XFree(data);

	return result;
}

static unsigned int
channel(unsigned long pixel, unsigned long mask)
{
	if (mask == 0)
		return 0;

	pixel &= mask;

	while ((mask & 1) == 0) {
		mask >>= 1;
		pixel >>= 1;
	}

	return pixel * 255 / mask;
}

static Pixmap
legacy_icon(struct client *c)
{
	XImage *alpha = NULL, *source = NULL;
	Visual *visual = NULL;
	XWMHints *hints = NULL;
	Pixmap mask, pixmap, result;
	Window root;
	int x, y;
	unsigned int border, depth, h, iy, md, mh, mw, w;
	unsigned char *rgba = NULL;

	hints = XGetWMHints(wm.display, c->window);

	if (hints == NULL)
		return None;

	pixmap = hints->flags & IconPixmapHint ? hints->icon_pixmap : None;
	mask = hints->flags & IconMaskHint ? hints->icon_mask : None;

	XFree(hints);

	if (pixmap == 0 ||
		XGetGeometry(wm.display, pixmap, &root, &x, &y, &w, &h, &border,
			&depth) == 0 ||
		w == 0 || h == 0 || w > MAX_ICON_DIMENSION ||
		h > MAX_ICON_DIMENSION ||
		(depth != 1 &&
			depth !=
				(unsigned int)DefaultDepth(
					wm.display, wm.screen)))
		return None;

	visual = DefaultVisual(wm.display, wm.screen);

	if (visual->class != TrueColor)
		return None;

	source = XGetImage(wm.display, pixmap, 0, 0, w, h, AllPlanes, ZPixmap);

	if (source == NULL)
		return None;

	if (mask &&
		XGetGeometry(wm.display, mask, &root, &x, &y, &mw, &mh, &border,
			&md) &&
		md == 1 && mw >= w && mh >= h)
		alpha = XGetImage(wm.display, mask, 0, 0, w, h, 1, ZPixmap);

	rgba = allocate_array(h, (size_t)w * 4);

	for (iy = 0; iy < h; iy++) {
		unsigned int ix;

		for (ix = 0; ix < w; ix++) {
			unsigned long pixel;
			unsigned char *p = NULL;

			pixel = XGetPixel(source, ix, iy);
			p = rgba + 4 * (iy * w + ix);

			p[0] = depth == 1 ? (pixel ? 0 : 255)
					: channel(pixel, visual->red_mask);
			p[1] = depth == 1 ? p[0]
					: channel(pixel, visual->green_mask);
			p[2] = depth == 1 ? p[0]
					: channel(pixel, visual->blue_mask);
			p[3] = alpha == NULL || XGetPixel(alpha, ix, iy) ? 255
									: 0;
		}
	}

	result = cache_rgba(c, rgba, w, h);

	free(rgba);
	XDestroyImage(source);

	if (alpha != NULL)
		XDestroyImage(alpha);

	return result;
}

static int
matches(const struct client *c, const char *name)
{
	return *name != '\0' &&
		((c->instance != NULL && strcasecmp(c->instance, name) == 0) ||
			(c->class_name != NULL &&
				strcasecmp(c->class_name, name) == 0));
}

static Pixmap
configured_icon(struct client *c)
{
	int i;

	for (i = 0; i < wm.config.icon_count; i++) {
		const struct desktop_icon *icon = NULL;
		Pixmap result;
		const char *base = NULL, *path = NULL;
		char command[sizeof(icon->command)];

		icon = &wm.config.icons[i];

		string_copy(command, icon->command, sizeof(command));
		command[strcspn(command, " \t")] = '\0';

		base = strrchr(command, '/');

		if (!matches(c, icon->id) &&
			!matches(c, (base != NULL) ? base + 1 : command))
			continue;

		if (*icon->image != '/') {
			c->taskbar_icon_width = c->taskbar_icon_height =
				taskbar_pixels(TASKBAR_ICON_SIZE);
			c->taskbar_icon_active = builtin_icon_pixmap(wm.display,
				wm.root, DefaultDepth(wm.display, wm.screen),
				taskbar_pixels(TASKBAR_ICON_SIZE),
				builtin_icon_parse(icon->image),
				wm.colors[LIGHT], wm.colors);
			return builtin_icon_pixmap(wm.display, wm.root,
				DefaultDepth(wm.display, wm.screen),
				taskbar_pixels(TASKBAR_ICON_SIZE),
				builtin_icon_parse(icon->image),
				wm.colors[FACE], wm.colors);
		}

		path = icon->image;

		result = wm_image_load_file(path, wm.colors[FACE],
			taskbar_pixels(TASKBAR_ICON_SIZE),
			&c->taskbar_icon_width, &c->taskbar_icon_height, 1);

		if (result == None)
			continue;

		c->taskbar_icon_active = wm_image_load_file(path,
			wm.colors[LIGHT], taskbar_pixels(TASKBAR_ICON_SIZE),
			&c->taskbar_icon_width, &c->taskbar_icon_height, 1);

		return result;
	}

	return None;
}

void
taskbar_icon_free(struct client *c)
{
	if (c->taskbar_icon)
		XFreePixmap(wm.display, c->taskbar_icon);

	if (c->taskbar_icon_active)
		XFreePixmap(wm.display, c->taskbar_icon_active);
	c->taskbar_icon = c->taskbar_icon_active = None;
}

void
taskbar_icon_update(struct client *c)
{
	taskbar_icon_free(c);
	c->taskbar_icon = configured_icon(c);

	if (c->taskbar_icon == 0)
		c->taskbar_icon = net_icon(c);

	if (c->taskbar_icon == 0)
		c->taskbar_icon = legacy_icon(c);
}

/* Shared fallback artwork belongs to the taskbar, not individual clients. */
void
taskbar_icons_init(void)
{
	int i;

	for (i = 0; i < 2; i++)
		generic_icons[i] = builtin_icon_pixmap(wm.display, wm.root,
			DefaultDepth(wm.display, wm.screen),
			taskbar_pixels(TASKBAR_ICON_SIZE), BUILTIN_APPLICATION,
			wm.colors[i ? LIGHT : FACE], wm.colors);
}

void
taskbar_icons_free(void)
{
	int i;

	for (i = 0; i < 2; i++) {
		if (generic_icons[i])
			XFreePixmap(wm.display, generic_icons[i]);
		generic_icons[i] = None;
	}
}

void
taskbar_icon_draw(struct client *c, Drawable target, int x, int y, int selected)
{
	Pixmap icon;
	unsigned int width, height;
	int size;

	size = taskbar_pixels(TASKBAR_ICON_SIZE);
	if (c->taskbar_icon) {
		icon = selected && c->taskbar_icon_active
			? c->taskbar_icon_active
			: c->taskbar_icon;
		width = c->taskbar_icon_width;
		height = c->taskbar_icon_height;
	} else {
		icon = generic_icons[selected != 0];
		width = height = size;
	}
	if (icon == None)
		return;
	XCopyArea(wm.display, icon, target, wm.gc, 0, 0, width, height,
		x + (size - (int)width) / 2, y + (size - (int)height) / 2);
}
