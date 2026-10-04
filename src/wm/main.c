#include <sys/wait.h>

#include <X11/Xproto.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "clock.h"
#include "events.h"
#include "settings.h"
#include "tray-widgets.h"
#include "wm.h"

enum {
	EVENT_BATCH_LIMIT = 256,
	MOTION_BATCH_LIMIT = 64
};

struct wm wm;

static volatile sig_atomic_t stopped;
static volatile sig_atomic_t signal_writer = -1;
static int signal_reader;

static int claiming, ownership_error;

static int	xerror(Display *, XErrorEvent *);
static void	signal_received(int);
static void	signals_init(void);
static int	timer_timeout(double);
static int	claim_display(void);
static void	adopt_windows(void);
static void	initialize(int);
static void	next_event(XEvent *);
static void	run_events(void);
static void	cleanup(void);

static int
xerror(Display *display, XErrorEvent *event)
{
	char message[256];

	if (claiming && event->error_code == BadAccess) {
		ownership_error = 1;
		return 0;
	}

	/*
	 * A client can disappear between any query and the following request.
	 */
	if (event->error_code == BadWindow ||
		event->error_code == BadDrawable ||
		(event->error_code == BadMatch &&
			(event->request_code == X_SetInputFocus ||
				event->request_code == X_ConfigureWindow)))
		return 0;

	XGetErrorText(display, event->error_code, message, sizeof(message));
	warnx("X11 error: %s (request %u, resource 0x%lx)", message,
		event->request_code, event->resourceid);

	return 0;
}

/* A full pipe already guarantees that poll will wake. */
static void
signal_received(int number)
{
	int saved_errno;
	ssize_t written;

	saved_errno = errno;
	if (number != SIGCHLD)
		stopped = 1;
	written = write(signal_writer, "", 1);
	(void)written;
	errno = saved_errno;
}

static void
signals_init(void)
{
	struct sigaction action = {0};
	int descriptors[2], i;

	if (pipe(descriptors) < 0)
		err(1, "signal pipe");
	for (i = 0; i < 2; i++)
		if (fcntl(descriptors[i], F_SETFL, O_NONBLOCK) < 0 ||
			fcntl(descriptors[i], F_SETFD, FD_CLOEXEC) < 0)
			err(1, "signal pipe flags");
	signal_reader = descriptors[0];
	signal_writer = descriptors[1];
	action.sa_handler = signal_received;
	sigemptyset(&action.sa_mask);
	sigaction(SIGINT, &action, NULL);
	sigaction(SIGTERM, &action, NULL);
	sigaction(SIGHUP, &action, NULL);
	action.sa_flags = SA_NOCLDSTOP;
	sigaction(SIGCHLD, &action, NULL);
}

static int
timer_timeout(double now)
{
	double deadlines[3], delay, next = 0;
	int i, milliseconds;

	deadlines[0] = tray_widgets_deadline();
	deadlines[1] = taskbar_tooltip_deadline();
	deadlines[2] = owm_session_deadline();
	for (i = 0; i < 3; i++)
		if (deadlines[i] > 0 && (next == 0 || deadlines[i] < next))
			next = deadlines[i];
	if (next == 0)
		return -1;
	delay = (next - now) * 1000;
	if (delay <= 0)
		return 0;
	if (delay >= INT_MAX)
		return INT_MAX;
	/* Round up so a submillisecond remainder cannot spin the loop. */
	milliseconds = (int)delay;

	return milliseconds + (delay > milliseconds);
}

static int
claim_display(void)
{
	XSetWindowAttributes a = {0};

	XSetErrorHandler(xerror);
	claiming = 1;
	XSelectInput(wm.display, wm.root,
		SubstructureRedirectMask | SubstructureNotifyMask |
			StructureNotifyMask | PropertyChangeMask |
			ButtonPressMask);
	XSync(wm.display, False);
	claiming = 0;

	if (ownership_error) {
		warnx("another window manager is already running on this "
		"display");
		return 0;
	}

	a.override_redirect = True;

	wm.support = XCreateWindow(wm.display, wm.root, -1, -1, 1, 1, 0,
		CopyFromParent, InputOutput, CopyFromParent, CWOverrideRedirect,
		&a);

	if (!wm_protocol_init())
		return 0;
	desktop_init();

	return 1;
}

static void
adopt_windows(void)
{
	Window *children = NULL, parent, root;
	unsigned int count;

	wm.scanning = 1;

	if (XQueryTree(
		wm.display, wm.root, &root, &parent, &children, &count)) {
		unsigned int i;

		for (i = 0; i < count; i++) {
			XWindowAttributes a;
			unsigned long n;
			int iconic;
			unsigned char *state = NULL;

			if (XGetWindowAttributes(wm.display, children[i], &a) ==
					0 ||
				a.override_redirect)
				continue;

			state = wm_property_get(children[i], wm.atoms[A_STATE],
				wm.atoms[A_STATE], 32, &n);
			iconic = state != NULL && n &&
				((long *)state)[0] == IconicState;

			if (state != NULL)
				XFree(state);

			if (a.map_state != IsUnmapped || iconic)
				window_client_manage(children[i]);
		}

		if (children != NULL)
			XFree(children);
	}

	wm.scanning = 0;
	window_client_focus_topmost();
}

static void
initialize(int restarting)
{
	signals_init();
	wm_draw_init();
	window_frame_init();
	taskbar_init();
	desktop_icons_init();

	if (wm.config.restore_session && !restarting)
		owm_session_load();
	desktop_publish();
	taskbar_draw();
	adopt_windows();
	owm_shortcuts_grab();
	owm_session_launch();
	wm.running = 1;
}

/* Reduce pointer lag without reordering motion and button releases. */
static void
next_event(XEvent *event)
{
	int skipped;

	XNextEvent(wm.display, event);

	if (event->type != MotionNotify)
		return;

	for (skipped = 0; skipped < MOTION_BATCH_LIMIT &&
		XEventsQueued(wm.display, QueuedAlready);
		skipped++) {
		XEvent next;

		XPeekEvent(wm.display, &next);

		if (next.type != MotionNotify ||
			next.xmotion.window != event->xmotion.window ||
			next.xmotion.state != event->xmotion.state)
			break;

		XNextEvent(wm.display, event);
	}
}

static void
run_events(void)
{
	while (wm.running && stopped == 0) {
		double now;
		int i, status;
		pid_t pid;

		/*
		 * Bound batches so continuous pointer/property traffic cannot
		 * starve timers.
		 */

		for (i = 0; i < EVENT_BATCH_LIMIT && XPending(wm.display) &&
			wm.running && stopped == 0;
			i++) {
			XEvent e;

			next_event(&e);

			wm_events_dispatch(&e);
		}

		now = monotonic_time();

		tray_widgets_update(now);
		taskbar_tooltip_update(now);
		owm_session_tick(now);

		while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
			owm_session_reaped(pid, status);
		XFlush(wm.display);

		if (XPending(wm.display) == 0 && wm.running && stopped == 0) {
			struct pollfd fds[2] = {{0}};
			char signals[64];

			fds[0].fd = ConnectionNumber(wm.display);
			fds[0].events = POLLIN;
			fds[1].fd = signal_reader;
			fds[1].events = POLLIN;

			if (poll(fds, 2, timer_timeout(monotonic_time())) < 0 &&
				errno != EINTR)
				break;

			while (read(signal_reader, signals, sizeof(signals)) >
				0)
				;

			if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL))
				break;
		}
	}
}

static void
cleanup(void)
{
	int writer;

	focus_cycle_free();
	window_move_finish(1);
	owm_menu_free();
	desktop_icons_free();
	window_client_cleanup();

	wm_protocol_cleanup();
	taskbar_free();
	XDestroyWindow(wm.display, wm.support);
	owm_session_free();
	owm_config_bindings_free(&wm.config);
	window_frame_free();
	wm_draw_free();
	XSync(wm.display, False);
	XCloseDisplay(wm.display);
	writer = signal_writer;
	signal_writer = -1;
	close(writer);
	close(signal_reader);
}

int
main(int argc, char *argv[])
{
	int desktop, restarting;
	const char *restart_desktop = NULL;

	if (argc > 1) {
		fprintf((strcmp(argv[1], "--help") == 0) ? stdout : stderr,
			"Usage: owm [--help]\nConfiguration: "
			"$XDG_CONFIG_HOME/owm "
			"or "
			"~/.config/owm\n");
		return argc == 2 && strcmp(argv[1], "--help") == 0 ? 0 : 1;
	}

	restart_desktop = getenv("OWM_RESTART_DESKTOP");
	desktop = 0;
	restarting = restart_desktop != NULL &&
		parse_integer(restart_desktop, 0, MAX_DESKTOPS - 1, &desktop);

	unsetenv("OWM_RESTART_DESKTOP");

	if (!owm_config_load(&wm.config)) {
		owm_config_bindings_free(&wm.config);
		return 1;
	}

	wm.display = XOpenDisplay(NULL);

	if (wm.display == NULL) {
		warnx("cannot open X11 display");
		owm_config_bindings_free(&wm.config);
		return 1;
	}

	fcntl(ConnectionNumber(wm.display), F_SETFD, FD_CLOEXEC);
	wm.screen = DefaultScreen(wm.display);
	wm.root = RootWindow(wm.display, wm.screen);
	wm.width = DisplayWidth(wm.display, wm.screen);
	wm.height = DisplayHeight(wm.display, wm.screen);

	if (!claim_display()) {
		XCloseDisplay(wm.display);
		owm_config_bindings_free(&wm.config);
		return 1;
	}

	initialize(restarting);
	run_events();
	cleanup();

	if (wm.restart) {
		/*
		 * Keep the PID so session managers keep waiting.
		 * Prefer a newly built binary, but retain the running
		 * executable as a fallback if its original path has
		 * disappeared.
		 */
		execvp(argv[0], argv);
		execl("/proc/self/exe", argv[0], (char *)NULL);
		warn("restart");
		return 1;
	}

	return 0;
}
