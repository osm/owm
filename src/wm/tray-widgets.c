#include <string.h>

#include "builtin-icons.h"
#include "string-util.h"
#include "tray-widgets.h"
#include "tray-wireguard.h"
#include "wm.h"

static struct {
	Pixmap		pixmap;

	unsigned int	width;
	unsigned int	height;

	int		source_x;
	int		source_y;
} widget_images[TRAY_ICON_COUNT];

static int	total_width(void);
static void	icon_bounds(int);
static void	create_icon(int, int, enum builtin_icon);
static void	network_update(struct tray_widget *, double);
static int	measure_widget(const struct tray_widget *);
static int	update_widget(struct tray_widget *, double);

static struct tray_widget widgets[] = {
	{"", "", TRAY_ICON_NONE, 0, 0, 20, tray_battery_update,
		&wm.config.tray_battery},
	{"", "", TRAY_ICON_NETWORK, 0, 0, 1, network_update,
		&wm.config.tray_wireguard},
	{"", "", TRAY_ICON_NONE, 0, 0, 1, tray_clock_update,
		&wm.config.tray_clock}};

int
tray_widget_icon_available(int index)
{
	return index >= 0 && index < TRAY_ICON_COUNT &&
		widget_images[index].pixmap != None;
}

/*
 * Center the visible artwork, rather than any padding in its image canvas.
 * Inspect once at startup. Drawing only copies the cached bounds.
 */
static void
icon_bounds(int index)
{
	XImage *image = NULL;
	int bottom, height, left, right, top, width, y;

	if (widget_images[index].pixmap == 0)
		return;

	width = (int)widget_images[index].width;
	height = (int)widget_images[index].height;
	image = XGetImage(wm.display, widget_images[index].pixmap, 0, 0, width,
		height, AllPlanes, ZPixmap);

	if (image == NULL)
		return;

	left = width;
	top = height;
	right = -1;
	bottom = -1;

	for (y = 0; y < height; y++) {
		int x;

		for (x = 0; x < width; x++) {
			if (XGetPixel(image, x, y) == wm.colors[FACE])
				continue;

			if (x < left)
				left = x;

			if (x > right)
				right = x;

			if (y < top)
				top = y;

			if (y > bottom)
				bottom = y;
		}
	}

	XDestroyImage(image);

	if (right < left || bottom < top)
		return;

	widget_images[index].source_x = left;
	widget_images[index].source_y = top;
	widget_images[index].width = (unsigned int)(right - left + 1);
	widget_images[index].height = (unsigned int)(bottom - top + 1);
}

static void
create_icon(int index, int size, enum builtin_icon builtin)
{
	widget_images[index].pixmap = builtin_icon_pixmap(wm.display, wm.root,
		DefaultDepth(wm.display, wm.screen), size, builtin,
		wm.colors[FACE], wm.colors);
	widget_images[index].width = size;
	widget_images[index].height = size;
}

void
tray_widgets_init(void)
{
	int i, size;

	size = taskbar_pixels(16);

	if (wm.config.tray_wireguard)
		create_icon(TRAY_ICON_NETWORK, size, BUILTIN_NETWORK);

	if (wm.config.tray_battery) {
		create_icon(TRAY_ICON_BATTERY, size, BUILTIN_BATTERY);
		create_icon(TRAY_ICON_POWER, size, BUILTIN_POWER);
	}

	for (i = 0; i < TRAY_ICON_COUNT; i++)
		icon_bounds(i);
}

void
tray_widgets_free(void)
{
	int i;

	tray_wireguard_close();

	for (i = 0; i < TRAY_ICON_COUNT; i++) {
		if (widget_images[i].pixmap)
			XFreePixmap(wm.display, widget_images[i].pixmap);
		widget_images[i].pixmap = None;
	}
}

static void
network_update(struct tray_widget *widget, double now)
{
	tray_wireguard_update(widget->text, sizeof(widget->text), now);
}

static int
measure_widget(const struct tray_widget *widget)
{
	int width;

	if (*widget->text == '\0')
		return 0;

	width = font_width(wm.font, widget->text, (int)strlen(widget->text)) +
		taskbar_pixels(16);

	if (widget->icon >= 0)
		width += taskbar_pixels(20);

	return width;
}

/* Return whether this widget changed the taskbar's visible content. */
static int
update_widget(struct tray_widget *widget, double now)
{
	int changed, old_icon;
	char old_text[sizeof(widget->text)];

	if (!*widget->enabled) {
		changed = widget->width != 0;
		widget->text[0] = widget->tooltip[0] = '\0';
		widget->width = 0;
		widget->next_update = 0;
		return changed;
	}

	if (now < widget->next_update)
		return 0;

	string_copy(old_text, widget->text, sizeof(old_text));
	old_icon = widget->icon;

	widget->update(widget, now);
	widget->next_update = now + widget->interval;

	if (strcmp(old_text, widget->text) == 0 && old_icon == widget->icon)
		return 0;

	widget->width = measure_widget(widget);

	return 1;
}

void
tray_widgets_update(double now)
{
	size_t i;
	int changed = 0, old_width;

	old_width = total_width();
	for (i = 0; i < sizeof(widgets) / sizeof(*widgets); i++)
		changed |= update_widget(&widgets[i], now);

	if (changed && wm.taskbar) {
		if (old_width != total_width())
			taskbar_draw();
		else
			taskbar_draw_tray();
	}
}

/* Zero means that no widget needs a timer. */
double
tray_widgets_deadline(void)
{
	size_t i;
	double next = 0;

	for (i = 0; i < sizeof(widgets) / sizeof(*widgets); i++) {
		if (!*widgets[i].enabled)
			continue;
		if (next == 0 || widgets[i].next_update < next)
			next = widgets[i].next_update;
	}

	return next;
}

static int
total_width(void)
{
	size_t i;
	int width = 0;

	for (i = 0; i < sizeof(widgets) / sizeof(*widgets); i++)
		width += widgets[i].width;

	return width;
}

int
tray_widgets_left(void)
{
	return wm.width - total_width() - 4;
}

void
tray_widgets_draw(void)
{
	size_t i;
	int height, x;

	x = tray_widgets_left();
	height = wm.config.taskbar_height;

	for (i = 0; i < sizeof(widgets) / sizeof(*widgets); i++) {
		int color, icon, icon_x, icon_y, inset, width;

		width = widgets[i].width;

		if (width == 0)
			continue;

		inset = 0;
		icon = widgets[i].icon;

		if (icon >= 0) {
			icon_x = x + taskbar_pixels(8) +
				(taskbar_pixels(16) -
					(int)widget_images[icon].width) /
					2;
			icon_y = 5 +
				(height - 9 - (int)widget_images[icon].height) /
					2;
			XCopyArea(wm.display, widget_images[icon].pixmap,
				wm.taskbar, wm.gc, widget_images[icon].source_x,
				widget_images[icon].source_y,
				widget_images[icon].width,
				widget_images[icon].height, icon_x, icon_y);
			inset = taskbar_pixels(20);
		}

		color = BLACK;
		if (icon == TRAY_ICON_NETWORK &&
			(strcmp(widgets[i].text, "off") == 0 ||
				strcmp(widgets[i].text, "?") == 0))
			color = SHADOW;

		wm_draw_text(wm.taskbar, x + taskbar_pixels(8) + inset, 5,
			width - taskbar_pixels(16) - inset, height - 9,
			widgets[i].text, wm.colors[color]);
		x += width;
	}
}

/* Hit testing uses the same origin and widths as tray_widgets_draw. */
const char *
tray_widgets_tooltip(int x, int y)
{
	size_t i;
	int left;

	if (y < 4 || y >= wm.config.taskbar_height - 3)
		return NULL;

	left = tray_widgets_left();

	for (i = 0; i < sizeof(widgets) / sizeof(*widgets); i++) {
		if (x >= left && x < left + widgets[i].width)
			return (*widgets[i].tooltip != '\0')
				? widgets[i].tooltip
				: NULL;
		left += widgets[i].width;
	}

	return NULL;
}
