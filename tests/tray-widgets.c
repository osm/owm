/*
 * Exercise sysfs discovery without depending on laptop hardware or an X
 * server.
 */
#include <sys/stat.h>

#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "../src/shared/string-util.h"
#include "../src/wm/tray-widgets.c"

struct wm wm;

static int measurements, redraws, tray_redraws, updates;
static char directory[] = "/tmp/owm-battery-test.XXXXXX";

static void	write_supply_field(const char *, const char *);
static void	expect_battery(const char *, int);
static void	test_widget_update(struct tray_widget *, double);
static void	test_widget_options(void);
static void	test_clock(void);
static void	test_battery(void);

int
taskbar_pixels(int pixels)
{
	return pixels; /* Unit fixtures use the default 28px bar. */
}

int
font_width(struct text_font *font, const char *text, int length)
{
	(void)font;
	(void)text;
	measurements++;

	return length * 8;
}

void
taskbar_draw(void)
{
	redraws++;
}

void
taskbar_draw_tray(void)
{
	tray_redraws++;
}

void
wm_draw_text(Window window, int x, int y, int width, int height,
	const char *text, unsigned long color)
{
	(void)window;
	(void)x;
	(void)y;
	(void)width;
	(void)height;
	(void)text;
	(void)color;
}

static void
write_supply_field(const char *name, const char *value)
{
	FILE *file = NULL;
	char path[PATH_MAX];

	snprintf(path, sizeof(path), "%s/%s", directory, name);

	file = fopen(path, "w");

	assert(file != NULL);
	assert(fputs(value, file) >= 0);
	assert(fclose(file) == 0);
}

static void
expect_battery(const char *text, int icon)
{
	tray_battery_update(&widgets[0], 0);
	assert(!strcmp(widgets[0].text, text));
	assert(widgets[0].icon == icon);
}

static void
test_widget_update(struct tray_widget *widget, double now)
{
	(void)now;
	updates++;
	string_copy(widget->text, "widget", sizeof(widget->text));
	string_copy(widget->tooltip, "widget", sizeof(widget->tooltip));
	widget->icon = TRAY_ICON_NONE;
}

static void
test_widget_options(void)
{
	int i, mask;

	wm.width = 1280;
	wm.config.taskbar_height = 28;
	for (i = 0; i < 3; i++) {
		widgets[i].update = test_widget_update;
		widgets[i].next_update = 0;
	}

	for (mask = 7; mask >= 0; mask--) {
		int enabled, left;

		wm.config.tray_battery = (mask & 1) != 0;
		wm.config.tray_wireguard = (mask & 2) != 0;
		wm.config.tray_clock = (mask & 4) != 0;
		updates = 0;
		tray_widgets_update(1000 + (7 - mask) * 30);
		enabled = 0;
		left = tray_widgets_left();
		for (i = 0; i < 3; i++) {
			if (!(mask & (1 << i))) {
				assert(widgets[i].width == 0);
				assert(!*widgets[i].text &&
					!*widgets[i].tooltip);
				assert(widgets[i].next_update == 0);
				continue;
			}
			enabled++;
			assert(widgets[i].width == 64);
			assert(tray_widgets_tooltip(left + 1, 10) != NULL);
			left += widgets[i].width;
		}
		assert(updates == enabled);
		assert(tray_widgets_left() == wm.width - enabled * 64 - 4);
		if (!enabled) {
			assert(tray_widgets_tooltip(wm.width - 10, 10) == NULL);
			assert(tray_widgets_deadline() == 0);
		} else {
			double now, interval;

			now = 1000 + (7 - mask) * 30;
			interval = mask & 6 ? 1 : 20;
			assert(tray_widgets_deadline() == now + interval);
		}
	}
}

static void
test_clock(void)
{
	struct tray_widget clock = {0};
	struct tm local;
	time_t before, after;
	char expected[128];

	before = time(NULL);

	tray_clock_update(&clock, 0);

	after = time(NULL);

	assert(localtime_r(&before, &local) != NULL);
	assert(strftime(expected, sizeof(expected), "%A, %d %B %Y", &local));

	if (strcmp(expected, clock.tooltip)) {
		assert(localtime_r(&after, &local) != NULL);
		assert(strftime(
			expected, sizeof(expected), "%A, %d %B %Y", &local));
		assert(!strcmp(expected, clock.tooltip));
	}
}

static void
test_battery(void)
{
	size_t i;
	int left, measured;
	char path[PATH_MAX];
	const char *supplies[] = {"Cell", "Adapter", "USB"};
	const char *fields[] = {"Cell/type", "Cell/capacity", "Cell/status",
		"Cell/present", "Adapter/type", "Adapter/online", "USB/type",
		"USB/online"};

	assert(mkdtemp(directory) != NULL);
	assert(setenv("OWM_POWER_SUPPLY_PATH", directory, 1) == 0);

	for (i = 0; i < sizeof(supplies) / sizeof(*supplies); i++) {
		snprintf(path, sizeof(path), "%s/%s", directory, supplies[i]);
		assert(mkdir(path, 0700) == 0);
	}

	write_supply_field("Cell/type", "Battery\n");
	write_supply_field("Cell/capacity", "83\n");
	write_supply_field("Cell/status", "Discharging\n");
	write_supply_field("Adapter/type", "Mains\n");
	write_supply_field("Adapter/online", "0\n");
	expect_battery("BAT 83%", TRAY_ICON_NONE);
	widget_images[TRAY_ICON_BATTERY].pixmap = 1;
	widget_images[TRAY_ICON_POWER].pixmap = 2;
	expect_battery("83%", TRAY_ICON_BATTERY);
	write_supply_field("Adapter/online", "1\n");
	write_supply_field("Cell/status", "Full\n");
	expect_battery("83%", TRAY_ICON_POWER);
	write_supply_field("Cell/status", "Not charging\n");
	expect_battery("83%", TRAY_ICON_POWER);
	write_supply_field("Adapter/online", "0\n");
	/* Explicit offline overrides stale battery status. */
	expect_battery("83%", TRAY_ICON_BATTERY);
	wm.config.tray_battery = wm.config.tray_wireguard =
		wm.config.tray_clock = 1;
	wm.taskbar = 1;
	widgets[0].text[0] = 0;
	tray_widgets_update(100);
	assert(widgets[0].width == 3 * 8 + 16 + 20);

	measured = measurements;
	left = tray_widgets_left();

	for (i = 0; i < 100; i++)
		assert(tray_widgets_left() == left);
	assert(measurements == measured);
	tray_widgets_update(100.5);
	assert(measurements == measured);
	redraws = tray_redraws = 0;
	write_supply_field("USB/type", "USB\n");
	write_supply_field("USB/online", "1\n");
	tray_widgets_update(121);
	/* A changed icon must redraw even when the percentage is unchanged. */
	assert(widgets[0].icon == TRAY_ICON_POWER && redraws == 0 &&
		tray_redraws == 1);
	write_supply_field("Adapter/online", "unknown\n");
	write_supply_field("USB/online", "unknown\n");
	write_supply_field("Cell/status", "Charging\n");
	/* Fall back to battery status when online fields are unusable. */
	expect_battery("83%", TRAY_ICON_POWER);
	widget_images[TRAY_ICON_POWER].pixmap = None;
	expect_battery("BAT 83%+", TRAY_ICON_NONE);
	write_supply_field("Cell/present", "0\n");
	redraws = tray_redraws = 0;
	tray_widgets_update(142);
	assert(redraws == 1 && tray_redraws == 0);
	assert(!*widgets[0].text && widgets[0].icon == TRAY_ICON_NONE &&
		widgets[0].width == 0);
	write_supply_field("Cell/present", "1\n");
	write_supply_field("Cell/capacity", "101\n");
	expect_battery("", TRAY_ICON_NONE);

	for (i = 0; i < sizeof(fields) / sizeof(*fields); i++) {
		snprintf(path, sizeof(path), "%s/%s", directory, fields[i]);
		assert(unlink(path) == 0);
	}

	for (i = 0; i < sizeof(supplies) / sizeof(*supplies); i++) {
		snprintf(path, sizeof(path), "%s/%s", directory, supplies[i]);
		assert(rmdir(path) == 0);
	}

	assert(rmdir(directory) == 0);
	expect_battery("", TRAY_ICON_NONE);
}

int
main(void)
{
	test_clock();
	test_battery();
	test_widget_options();
	puts("PASS tray-widgets");

	return 0;
}
