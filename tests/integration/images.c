#include <png.h>

#include "../../src/shared/font.h"
#include "cases.h"
#include "harness.h"

static void	create_icon_images(void);
static void	create_large_icon(const char *);
static void	check_icon_artwork(Window);
static void	check_icon_group_drag(Window, Window);

static unsigned long	taskbar_icon_rgb(int);

static void
create_icon_images(void)
{
	FILE *file = NULL;
	png_image png = {0};
	char image_path[PATH_MAX + 32];
	unsigned char rgba[] = {255, 0, 0, 255, 0, 0, 255, 128, 255, 0, 0, 0};

	snprintf(image_path, sizeof(image_path), "%s/owm/test.xpm",
		getenv("XDG_CONFIG_HOME"));

	file = fopen(image_path, "w");

	CHECK(file);
	fputs("/* XPM */\n"
	"static char *test[] = {\n"
	"\"2 2 2 1\",\n"
	"\". c #ff0000\",\n"
	"\"  c None\",\n"
	"\". \",\n"
	"\" .\"};\n",
		file);
	CHECK(fclose(file) == 0);
	snprintf(image_path, sizeof(image_path), "%s/owm/test.png",
		getenv("XDG_CONFIG_HOME"));
	png.version = PNG_IMAGE_VERSION;
	png.width = 3;
	png.height = 1;
	png.format = PNG_FORMAT_RGBA;

	CHECK(png_image_write_to_file(&png, image_path, 0, rgba, 0, NULL));
	png_image_free(&png);
}

static void
create_large_icon(const char *path)
{
	png_image source = {0};
	unsigned char *rgba = NULL;
	int x, y;

	source.version = PNG_IMAGE_VERSION;
	source.width = 256;
	source.height = 256;
	source.format = PNG_FORMAT_RGBA;
	rgba = calloc(256 * 256, 4);

	CHECK(rgba);
	for (y = 0; y < 256; y++) {
		for (x = 0; x < 128; x++) {
			rgba[(y * 256 + x) * 4] = 255;
			rgba[(y * 256 + x) * 4 + 3] = 255;
		}
	}
	CHECK(png_image_write_to_file(&source, path, 0, rgba, 0, NULL));
	png_image_free(&source);
	free(rgba);
}

static void
check_icon_artwork(Window terminal)
{
	XImage *pixels = NULL;
	XColor colors[3];
	unsigned long red, transparent;
	Window png_icon;
	int i;

	/*
	 * Actual XPM pixels and its transparent mask, not just a successful
	 * load.
	 */
	pixels = XGetImage(dpy, terminal, (attributes(terminal).width - 2) / 2,
		23, 2, 1, AllPlanes, ZPixmap);

	CHECK(pixels);

	red = XGetPixel(pixels, 0, 0);
	transparent = XGetPixel(pixels, 1, 0);

	CHECK((red & pixels->red_mask) == pixels->red_mask &&
		!(red & pixels->green_mask) && !(red & pixels->blue_mask));
	CHECK(transparent != red);
	XDestroyImage(pixels);

	png_icon = find_icon("png");

	CHECK(png_icon && find_icon("broken-png"));
	pixels = XGetImage(dpy, png_icon, (attributes(png_icon).width - 3) / 2,
		23, 3, 1, AllPlanes, ZPixmap);
	CHECK(pixels);

	for (i = 0; i < 3; i++)
		colors[i].pixel = XGetPixel(pixels, i, 0);

	XQueryColors(dpy, DefaultColormap(dpy, DefaultScreen(dpy)), colors, 3);
	CHECK(colors[0].red == 65535 && !colors[0].green && !colors[0].blue);
	CHECK(colors[1].red / 257 == 31 && colors[1].green / 257 == 40 &&
		colors[1].blue / 257 == 173);
	CHECK(colors[2].red / 257 == 62 && colors[2].green / 257 == 81 &&
		colors[2].blue / 257 == 91);
	XDestroyImage(pixels);
}

static void
check_icon_group_drag(Window terminal, Window command)
{
	XWindowAttributes group_before, placed;
	int grid_x, grid_y;

	placed = attributes(terminal);
	grid_x = placed.width + 8;
	grid_y = placed.height + 8;
	group_before = attributes(command);

	/*
	 * A desktop rectangle selects both icons, and dragging one moves the
	 * group.
	 */
	drag(0, 0, placed.x + placed.width + 4, placed.y + placed.height + 4);
	drag(placed.x + 30, placed.y + 20, grid_x + 12, grid_y + 9);
	WAIT(attributes(terminal).x == placed.x + grid_x &&
		attributes(terminal).y == placed.y + grid_y);
	CHECK(attributes(command).x == group_before.x + grid_x &&
		attributes(command).y == group_before.y + grid_y);
	placed = attributes(terminal);
	group_before = attributes(command);

	/*
	 * Clicking empty desktop clears the group. The next drag moves only
	 * Terminal.
	 */
	click(1200, 700);
	drag(placed.x + 30, placed.y + 20, grid_x, 0);
	WAIT(attributes(terminal).x == placed.x + grid_x);
	CHECK(attributes(command).x == group_before.x &&
		attributes(command).y == group_before.y);
	placed = attributes(terminal);

	/*
	 * Dropping outside the work area clamps the complete icon above the
	 * taskbar.
	 */
	drag(placed.x + 30, placed.y + 20, 1100, 650);
	placed = attributes(terminal);
	CHECK(placed.x >= 0 && placed.x + placed.width <= 1280 &&
		placed.y + placed.height <= 772);
}

void
test_icons(void)
{
	FILE *file = NULL;
	XWindowAttributes cmd, original, placed;
	Window command, terminal;
	int grid_x, grid_y;
	char contents[32] = {0};
	char marker[PATH_MAX + 32], positions[PATH_MAX + 32];

	static const char invalid_icons[] =
		"desktop_icon = \"terminal | Duplicate | false | \"\n"
		"desktop_icon = \"bad id | Invalid | false | \"\n"
		"desktop_icon = \"empty | No command | | \"\n"
		"desktop_icon = \"relative | Invalid | true | "
		"icons/test.png\"\n"
		"desktop_icon = \"tilde | Invalid | true | ~/test.png\"\n";

	snprintf(positions, sizeof(positions), "%s/owm/desktop-icon-positions",
		getenv("XDG_CONFIG_HOME"));
	snprintf(marker, sizeof(marker), "%s/icon-command", output_path);
	create_icon_images();
	write_config(
		"restore_session = false\n"
		"desktop_icon = \"png | PNG | true | %s/owm/test.png\"\n"
		"desktop_icon = \"broken-png | Fallback | true | "
		"/missing.png\"\n"
		"desktop_icon = \"terminal | Terminal | xterm -name "
		"icon-launch | "
		"%s/owm/test.xpm\"\n"
		"desktop_icon = \"command | Command | printf icon-ok | cat > "
		"$OWM_TEST_OUTPUT/icon-command | "
		"/missing.xpm\"\n"
		"%s",
		getenv("XDG_CONFIG_HOME"), getenv("XDG_CONFIG_HOME"),
		invalid_icons);
	file = fopen(positions, "w");
	CHECK(file);
	fputs("unknown 20 20\n"
	"terminal invalid 50\n"
	"terminal -10 20\n"
	"command 200 40 trailing\n",
		file);
	CHECK(fclose(file) == 0);
	start_wm();
	WAIT(find_icon("terminal") && find_icon("command"));
	CHECK(!find_icon("bad id") && !find_icon("empty") &&
		!find_icon("relative") && !find_icon("tilde") &&
		count_clients() == 0);

	terminal = find_icon("terminal");
	command = find_icon("command");
	original = attributes(terminal);

	CHECK(original.override_redirect && original.map_state == IsViewable);

	check_icon_artwork(terminal);
	click(original.x + 30, original.y + 20);
	CHECK(count_clients() == 0);
	pause_ms(400);
	drag(original.x + 30, original.y + 20, 200, 130);

	grid_x = original.width + 8;
	grid_y = original.height + 8;

	WAIT(attributes(terminal).x ==
			original.x + ((200 + grid_x / 2) / grid_x) * grid_x &&
		attributes(terminal).y ==
			original.y + ((130 + grid_y / 2) / grid_y) * grid_y);
	CHECK(count_clients() == 0);

	placed = attributes(terminal);

	CHECK(access(positions, R_OK) == 0);
	right_click(placed.x + 30, placed.y + 20);
	WAIT(desktop_menu());
	shortcut(XK_Escape, 0);
	WAIT(!desktop_menu());
	click(placed.x + 30, placed.y + 20);
	click(placed.x + 30, placed.y + 20);
	WAIT(find_instance("icon-launch"));
	CHECK(stack_position(parent_of(find_instance("icon-launch"))) >
		stack_position(terminal));
	message(find_instance("icon-launch"), "_NET_CLOSE_WINDOW", CurrentTime,
		0, 0);
	WAIT(count_clients() == 0);

	cmd = attributes(command);

	click(cmd.x + 30, cmd.y + 20);
	click(cmd.x + 30, cmd.y + 20);
	WAIT(access(marker, R_OK) == 0);
	file = fopen(marker, "r");
	CHECK(file);

	CHECK(fread(contents, 1, sizeof(contents) - 1, file) == 7);
	CHECK(!strcmp(contents, "icon-ok"));
	fclose(file);
	message(root, "_NET_CURRENT_DESKTOP", 1, CurrentTime, 0);
	WAIT(value(root, "_NET_CURRENT_DESKTOP", XA_CARDINAL) == 1);
	CHECK(attributes(terminal).map_state == IsViewable);
	CHECK(attributes(terminal).x == placed.x &&
		attributes(terminal).y == placed.y);
	screenshot("desktop-icons");
	stop_wm();
	CHECK(!find_icon("terminal"));

	/* IDs, not config order or labels, identify saved positions. */
	write_config("restore_session = false\n"
		"desktop_icon = \"command | Command | true | \"\n"
		"desktop_icon = \"terminal | Renamed | xterm -name "
		"icon-launch | "
		"\"\n");
	start_wm();
	WAIT(find_icon("terminal"));
	terminal = find_icon("terminal");
	CHECK(attributes(terminal).x == placed.x &&
		attributes(terminal).y == placed.y);
	command = find_icon("command");

	check_icon_group_drag(terminal, command);
	stop_wm();
	write_config("restore_session = false\n");
	start_wm();
	CHECK(!find_icon("terminal") && !find_icon("command"));
	stop_wm();
}

void
test_desktop_icon_scaling(void)
{
	int previous_art, previous_height, size;
	char large_path[PATH_MAX];

	create_icon_images();
	snprintf(large_path, sizeof(large_path), "%s/owm/large.png",
		getenv("XDG_CONFIG_HOME"));
	create_large_icon(large_path);
	previous_art = 0;
	previous_height = 0;

	for (size = 10; size <= 18; size += 8) {
		struct text_font *font = NULL;
		XImage *image = NULL;
		XWindowAttributes bounds;
		unsigned long background;
		Window browser, large, png, xpm;
		int area, left, length, right, side, y;
		char name[64];

		write_config(
			"restore_session = false\n"
			"font = \"Liberation Mono:size=%d\"\n"
			"desktop_icon = \"browser | Google Chrome | true | \"\n"
			"desktop_icon = \"scaled-xpm | XPM | true | "
			"%s/owm/test.xpm\"\n"
			"desktop_icon = \"scaled-png | PNG | true | "
			"%s/owm/test.png\"\n"
			"desktop_icon = \"large-png | Large PNG | true | "
			"%s\"\n",
			size, getenv("XDG_CONFIG_HOME"),
			getenv("XDG_CONFIG_HOME"), large_path);
		start_wm();
		WAIT(find_icon("browser") && find_icon("scaled-xpm") &&
			find_icon("scaled-png"));
		pause_ms(100);

		snprintf(name, sizeof(name), "Liberation Mono:size=%d", size);
		font = font_open(dpy, name);

		CHECK(font);
		browser = find_icon("browser");
		bounds = attributes(browser);
		area = bounds.height - font->ascent - font->descent - 6;

		CHECK(bounds.width >=
			font_width(font, "Google Chrome", 13) + 8);
		CHECK(bounds.height > previous_height);
		previous_height = bounds.height;
		font_close(font);

		image = XGetImage(dpy, browser, 0, 0, bounds.width, area,
			AllPlanes, ZPixmap);
		CHECK(image);
		background = XGetPixel(image, 0, 0);
		left = bounds.width;
		right = -1;

		for (y = 0; y < area; y++) {
			int x;
			for (x = 0; x < bounds.width; x++) {
				if (XGetPixel(image, x, y) == background)
					continue;
				if (x < left)
					left = x;
				if (x > right)
					right = x;
			}
		}

		CHECK(right - left + 1 > previous_art);
		previous_art = right - left + 1;
		XDestroyImage(image);

		/*
		 * Enlarged XPM masks still leave the transparent corner intact.
		 */
		xpm = find_icon("scaled-xpm");
		side = size == 10 ? 2 : 3;

		image = XGetImage(dpy, xpm, (bounds.width - side) / 2,
			(area - side) / 2, side, side, AllPlanes, ZPixmap);
		CHECK(image);
		CHECK(XGetPixel(image, 0, 0) == 0xff0000);
		CHECK(XGetPixel(image, side - 1, 0) == background);
		CHECK(XGetPixel(image, side - 1, side - 1) == 0xff0000);
		XDestroyImage(image);

		png = find_icon("scaled-png");
		length = size == 10 ? 3 : 5;

		image = XGetImage(dpy, png, (bounds.width - length) / 2,
			(area - 1) / 2, length, 1, AllPlanes, ZPixmap);
		CHECK(image);
		CHECK(XGetPixel(image, 0, 0) == 0xff0000);
		CHECK(XGetPixel(image, length / 2, 0) == 0x1f28ad);
		CHECK(XGetPixel(image, length - 1, 0) == background);
		XDestroyImage(image);
		large = find_icon("large-png");

		CHECK(large);
		image = XGetImage(dpy, large, (bounds.width - area) / 2, 0,
			area, area, AllPlanes, ZPixmap);
		CHECK(image);
		CHECK(XGetPixel(image, 1, area / 2) == 0xff0000);
		CHECK(XGetPixel(image, area - 2, area / 2) == background);
		XDestroyImage(image);

		screenshot(
			size == 10 ? "desktop-labels" : "desktop-large-labels");
		stop_wm();
	}
}

void
test_taskbar_icons(void)
{
	XWMHints hints = {0};
	XColor green;
	GC gc = NULL;
	Pixmap legacy;
	unsigned long invalid[] = {0xffffffff, 0xffffffff, 0};
	unsigned long pixels[2 + 32 * 32 + 2 + 16 * 16];
	Window window, other;
	XClassHint klass = {"configured", "OwmTest"};
	unsigned int i;

	create_icon_images();
	write_config("taskbar_hide_empty_desktops = false\ndesktops = 4\n"
		"desktop_icon = \"configured | Configured | true | "
		"%s/owm/test.png\"\n"
		"desktop_icon = \"server | Server | true | terminal\"\n",
		getenv("XDG_CONFIG_HOME"));
	start_wm();
	window = make_window("taskbar-icon", "Taskbar icon", 0);
	XMapWindow(dpy, window);
	XFlush(dpy);
	WAIT(count_clients() == 1);
	/* Fallback padding must match both selected and unselected buttons. */
	WAIT(taskbar_icon_rgb(110) == 0xd6d2c7);
	other = make_window("fallback-focus", "Other window", 0);
	XMapWindow(dpy, other);
	XFlush(dpy);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == other);
	WAIT(taskbar_icon_rgb(110) == 0xaca99f);
	XDestroyWindow(dpy, other);
	XFlush(dpy);
	WAIT(count_clients() == 1);
	WAIT(taskbar_icon_rgb(110) == 0xd6d2c7);

	pixels[0] = pixels[1] = 32;

	for (i = 2; i < 2 + 32 * 32; i++)
		pixels[i] = 0xff0000ff;
	pixels[2 + 32 * 32] = pixels[3 + 32 * 32] = 16;

	for (i = 4 + 32 * 32; i < sizeof(pixels) / sizeof(*pixels); i++)
		pixels[i] = 0xffff0000;
	XChangeProperty(dpy, window, atom("_NET_WM_ICON"), XA_CARDINAL, 32,
		PropModeReplace, (unsigned char *)pixels,
		sizeof(pixels) / sizeof(*pixels));
	XFlush(dpy);
	WAIT(taskbar_icon_rgb(114) == 0xff0000); /* Native 16px variant wins. */
	for (i = 2; i < 2 + 32 * 32; i++)
		pixels[i] = 0x800000ff;
	XChangeProperty(dpy, window, atom("_NET_WM_ICON"), XA_CARDINAL, 32,
		PropModeReplace, (unsigned char *)pixels, 2 + 32 * 32);
	XFlush(dpy);
	WAIT(taskbar_icon_rgb(114) ==
		0x6b69e3); /* 50% blue over the active button. */
	other = make_window("icon-focus", "Other window", 0);
	XMapWindow(dpy, other);
	XFlush(dpy);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == other);
	WAIT(taskbar_icon_rgb(114) == 0x5654cf); /* Inactive background. */
	message(window, "_NET_ACTIVE_WINDOW", 1, CurrentTime, 0);
	WAIT(value(root, "_NET_ACTIVE_WINDOW", XA_WINDOW) == window);
	WAIT(taskbar_icon_rgb(114) == 0x6b69e3);
	XDestroyWindow(dpy, other);
	XFlush(dpy);
	WAIT(count_clients() == 1);

	/* Transparent pixels expose the task icon background. */
	for (i = 2; i < 2 + 32 * 32; i++)
		pixels[i] = 0;
	XChangeProperty(dpy, window, atom("_NET_WM_ICON"), XA_CARDINAL, 32,
		PropModeReplace, (unsigned char *)pixels, 2 + 32 * 32);
	XFlush(dpy);
	WAIT(taskbar_icon_rgb(114) == 0xd6d2c7);

	XChangeProperty(dpy, window, atom("_NET_WM_ICON"), XA_CARDINAL, 32,
		PropModeReplace, (unsigned char *)invalid, 3);
	XFlush(dpy);
	WAIT(taskbar_icon_rgb(114) ==
		0xf6f3e8); /* Malformed artwork uses generic fallback. */
	XDeleteProperty(dpy, window, atom("_NET_WM_ICON"));

	legacy = XCreatePixmap(
		dpy, root, 16, 16, DefaultDepth(dpy, DefaultScreen(dpy)));
	gc = XCreateGC(dpy, legacy, 0, NULL);

	CHECK(XParseColor(dpy, DefaultColormap(dpy, DefaultScreen(dpy)),
		"#00ff00", &green));
	CHECK(XAllocColor(
		dpy, DefaultColormap(dpy, DefaultScreen(dpy)), &green));
	XSetForeground(dpy, gc, green.pixel);
	XFillRectangle(dpy, legacy, gc, 0, 0, 16, 16);

	hints.flags = IconPixmapHint;
	hints.icon_pixmap = legacy;

	XSetWMHints(dpy, window, &hints);
	XFlush(dpy);
	WAIT(taskbar_icon_rgb(114) == 0x00ff00);
	XSetClassHint(dpy, window, &klass);
	XFlush(dpy);
	WAIT(taskbar_icon_rgb(114) == 0xff0000);
	for (i = 2; i < 2 + 32 * 32; i++)
		pixels[i] = 0xff0000ff;
	XChangeProperty(dpy, window, atom("_NET_WM_ICON"), XA_CARDINAL, 32,
		PropModeReplace, (unsigned char *)pixels, 2 + 32 * 32);
	XFlush(dpy);
	WAIT(taskbar_icon_rgb(114) == 0xff0000);
	klass.res_name = "server";
	XSetClassHint(dpy, window, &klass);
	XFlush(dpy);
	WAIT(taskbar_icon_rgb(122) == 0x252722);
	klass.res_name = "taskbar-icon";
	XSetClassHint(dpy, window, &klass);
	XFlush(dpy);
	WAIT(taskbar_icon_rgb(114) == 0x0000ff);
	XDeleteProperty(dpy, window, atom("_NET_WM_ICON"));
	XFlush(dpy);
	WAIT(taskbar_icon_rgb(114) == 0x00ff00);
	XDeleteProperty(dpy, window, XA_WM_HINTS);
	XFlush(dpy);
	WAIT(taskbar_icon_rgb(114) == 0xf6f3e8);
	XFreeGC(dpy, gc);
	XFreePixmap(dpy, legacy);
	XFreeColors(dpy, DefaultColormap(dpy, DefaultScreen(dpy)), &green.pixel,
		1, 0);
	XDestroyWindow(dpy, window);
	XFlush(dpy);
	WAIT(count_clients() == 0);
	stop_wm();
}

static unsigned long
taskbar_icon_rgb(int x)
{
	XImage *image = NULL;
	XColor color = {0};
	Window bar;

	bar = taskbar_window();
	image = XGetImage(dpy, bar, x, attributes(bar).height / 2, 1, 1,
		AllPlanes, ZPixmap);

	CHECK(image);

	color.pixel = XGetPixel(image, 0, 0);

	XQueryColor(dpy, DefaultColormap(dpy, DefaultScreen(dpy)), &color);
	XDestroyImage(image);

	return ((unsigned long)(color.red / 257) << 16) |
		((color.green / 257) << 8) | color.blue / 257;
}
