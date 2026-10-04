/*
 * Exercise the SDL2 X11/OpenGL path used by games, on the isolated test
 * server.
 */
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_syswm.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                       \
	do {                                                                   \
		if (!(condition)) {                                            \
			fprintf(stderr,                                        \
				"SDL fullscreen failure at %d: %s (%s)\n",     \
				__LINE__, #condition, SDL_GetError());         \
			exit(1);                                               \
		}                                                              \
	} while (0)

static int (*fake_key)(Display *, unsigned int, Bool, unsigned long);
static int (*fake_button)(Display *, unsigned int, Bool, unsigned long);

static void	pump(void);
static void	check_mouse_buttons(Display *);
static void	check_keyboard_switches(SDL_Window *, Display *);
static int	is_fullscreen(Display *, Window);
static void	switch_desktop(Display *, int);
static void	check_rendering(SDL_Window *, Display *, Window);

static void
pump(void)
{
	int i;

	for (i = 0; i < 20; i++) {
		SDL_Event event;

		while (SDL_PollEvent(&event))
			continue;

		SDL_Delay(10);
	}
}

static void
check_mouse_buttons(Display *display)
{
	SDL_Event event;
	int i, pressed = 0, released = 0;

	CHECK(fake_button(display, Button1, True, 0));
	CHECK(fake_button(display, Button1, False, 0));
	XFlush(display);

	for (i = 0; i < 100 && !(pressed && released); i++) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_MOUSEBUTTONDOWN &&
				event.button.button == SDL_BUTTON_LEFT)
				pressed++;
			if (event.type == SDL_MOUSEBUTTONUP &&
				event.button.button == SDL_BUTTON_LEFT)
				released++;
		}
		SDL_Delay(10);
	}

	CHECK(pressed == 1 && released == 1);
}

static void
check_keyboard_switches(SDL_Window *window, Display *display)
{
	KeyCode super, number;
	int i, desktop;

	super = XKeysymToKeycode(display, XK_Super_L);
	CHECK(super != 0);

	for (i = 0; i < 2; i++) {
		for (desktop = 1; desktop >= 0; desktop--) {
			number = XKeysymToKeycode(display, XK_1 + desktop);
			CHECK(number != 0);
			CHECK(fake_key(display, super, True, 0));
			CHECK(fake_key(display, number, True, 0));
			XFlush(display);
			if (i % 2)
				pump();
			CHECK(fake_key(display, number, False, 0));
			CHECK(fake_key(display, super, False, 0));
			XFlush(display);
			pump();
			pump();
			CHECK(!!(SDL_GetWindowFlags(window) &
				SDL_WINDOW_INPUT_FOCUS) == !desktop);
		}
		check_mouse_buttons(display);
	}
}

static int
is_fullscreen(Display *display, Window window)
{
	Atom actual, fullscreen, property;
	unsigned long after, count;
	int format, found;
	unsigned char *data = NULL;

	property = XInternAtom(display, "_NET_WM_STATE", False);
	fullscreen = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);

	XGetWindowProperty(display, window, property, 0, 128, False, XA_ATOM,
		&actual, &format, &count, &after, &data);

	found = 0;

	if (actual == XA_ATOM && format == 32) {
		unsigned long i;

		for (i = 0; i < count; i++)
			found |= ((Atom *)data)[i] == fullscreen;
	}

	if (data != NULL)
		XFree(data);

	return found;
}

static void
switch_desktop(Display *display, int desktop)
{
	XEvent event = {0};
	Window root;

	root = DefaultRootWindow(display);

	event.xclient.type = ClientMessage;
	event.xclient.window = root;
	event.xclient.message_type =
		XInternAtom(display, "_NET_CURRENT_DESKTOP", False);
	event.xclient.format = 32;
	event.xclient.data.l[0] = desktop;
	event.xclient.data.l[1] = CurrentTime;
	XSendEvent(display, root, False,
		SubstructureRedirectMask | SubstructureNotifyMask, &event);
	XFlush(display);

	/*
	 * SDL defers focus changes. Allow that timer and its WM requests to
	 * settle before checking or switching back.
	 */
	pump();
	pump();
	pump();
}

/*
 * A mapped GL window is not sufficient: verify that swapped pixels actually
 * reach the screen. This catches regressions hidden by checks of geometry
 * alone.
 */
static void
check_rendering(SDL_Window *window, Display *display, Window xwindow)
{
	XImage *image = NULL;
	unsigned long pixel;
	void *proc = NULL;
	Window child;
	int x, y;
	void (*clear)(GLbitfield) = NULL;
	void (*clear_color)(GLfloat, GLfloat, GLfloat, GLfloat) = NULL;
	void (*finish)(void) = NULL;

	proc = SDL_GL_GetProcAddress("glClearColor");

	memcpy(&clear_color, &proc, sizeof(clear_color));
	proc = SDL_GL_GetProcAddress("glClear");
	memcpy(&clear, &proc, sizeof(clear));
	proc = SDL_GL_GetProcAddress("glFinish");
	memcpy(&finish, &proc, sizeof(finish));
	CHECK(clear_color && clear && finish);
	clear_color(1, 0, 0, 1);
	clear(GL_COLOR_BUFFER_BIT);
	SDL_GL_SwapWindow(window);
	finish();
	pump();

	CHECK(XTranslateCoordinates(display, xwindow,
		DefaultRootWindow(display), 20, 20, &x, &y, &child));

	image = XGetImage(display, DefaultRootWindow(display), x, y, 1, 1,
		AllPlanes, ZPixmap);

	CHECK(image);

	pixel = XGetPixel(image, 0, 0);

	CHECK((pixel & image->red_mask) == image->red_mask &&
		!(pixel & image->green_mask) && !(pixel & image->blue_mask));
	XDestroyImage(image);
}

int
main(void)
{
	SDL_SysWMinfo info;
	Display *display = NULL;
	SDL_GLContext context = NULL;
	void *xtst = NULL, *proc = NULL;
	SDL_Window *window = NULL;
	Window xwindow;
	int i;

	CHECK(SDL_setenv("SDL_VIDEODRIVER", "x11", 1) == 0);

	/* ezQuake explicitly enables this, regardless of SDL's default. */
	CHECK(SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "1"));
	CHECK(SDL_Init(SDL_INIT_VIDEO) == 0);
	xtst = SDL_LoadObject("libXtst.so.6");
	CHECK(xtst);
	proc = SDL_LoadFunction(xtst, "XTestFakeKeyEvent");
	CHECK(proc);
	memcpy(&fake_key, &proc, sizeof(fake_key));
	proc = SDL_LoadFunction(xtst, "XTestFakeButtonEvent");
	CHECK(proc);
	memcpy(&fake_button, &proc, sizeof(fake_button));

	window = SDL_CreateWindow("SDL fullscreen test", 100, 120, 640, 480,
		SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

	CHECK(window);

	context = SDL_GL_CreateContext(window);

	CHECK(context);

	SDL_VERSION(&info.version);
	CHECK(SDL_GetWindowWMInfo(window, &info));
	CHECK(info.subsystem == SDL_SYSWM_X11);

	display = info.info.x11.display;
	xwindow = info.info.x11.window;

	pump();
	check_rendering(window, display, xwindow);

	for (i = 0; i < 2; i++) {
		XWindowAttributes attrs, root_attrs;

		CHECK(SDL_SetWindowFullscreen(window,
			i ? SDL_WINDOW_FULLSCREEN
				: SDL_WINDOW_FULLSCREEN_DESKTOP) == 0);
		pump();
		CHECK(is_fullscreen(display, xwindow));

		CHECK(XGetWindowAttributes(display, xwindow, &attrs));
		CHECK(XGetWindowAttributes(
			display, DefaultRootWindow(display), &root_attrs));
		CHECK(attrs.x == 0 && attrs.y == 0 &&
			attrs.width == root_attrs.width &&
			attrs.height == root_attrs.height);
		CHECK(SDL_SetRelativeMouseMode(SDL_TRUE) == 0);
		CHECK(SDL_GetRelativeMouseMode() == SDL_TRUE);
		check_rendering(window, display, xwindow);
		switch_desktop(display, 1);
		CHECK(XGetWindowAttributes(display, xwindow, &attrs));
		CHECK(attrs.map_state != IsViewable);
		switch_desktop(display, 0);
		CHECK(XGetWindowAttributes(display, xwindow, &attrs));
		CHECK(attrs.map_state == IsViewable);
		CHECK(!(SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED));
		CHECK(SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS);
		CHECK(is_fullscreen(display, xwindow));
		check_rendering(window, display, xwindow);
		check_mouse_buttons(display);
		check_keyboard_switches(window, display);
		CHECK(SDL_SetRelativeMouseMode(SDL_FALSE) == 0);
		CHECK(SDL_SetWindowFullscreen(window, 0) == 0);
		pump();
		CHECK(!is_fullscreen(display, xwindow));
		CHECK(XGetWindowAttributes(display, xwindow, &attrs));
		CHECK(attrs.width == 640 && attrs.height == 480);
		check_rendering(window, display, xwindow);
	}

	SDL_GL_DeleteContext(context);
	SDL_DestroyWindow(window);
	SDL_Quit();
	SDL_UnloadObject(xtst);

	return 0;
}
