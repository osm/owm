#include "../../src/shared/font.h"
#include "../../src/shared/ui-config.h"
#include "cases.h"
#include "harness.h"

static void	run_paste(Window dialog, Window owner, Atom selection,
	Atom encoding, const char *text);
static void	check_run_argument(const char *expected);

void
test_run_dialog(void)
{
	XEvent unrelated = {0};
	XWindowAttributes frame;
	Window dialog;

	CHECK(access(session_path, F_OK) < 0 && errno == ENOENT);
	start_wm();
	shortcut(XK_p, Mod4Mask);
	WAIT(find_instance("owm-run"));

	dialog = find_instance("owm-run");

	CHECK(state(dialog, "_NET_WM_STATE_SKIP_TASKBAR"));

	unrelated.xclient.type = ClientMessage;
	unrelated.xclient.window = dialog;
	unrelated.xclient.message_type = atom("_OWM_TEST_UNRELATED");
	unrelated.xclient.format = 32;
	unrelated.xclient.data.l[0] = atom("WM_DELETE_WINDOW");
	XSendEvent(dpy, dialog, False, NoEventMask, &unrelated);
	XSync(dpy, False);
	pause_ms(100);
	CHECK(find_instance("owm-run") == dialog);
	CHECK(wait_exit(spawn("./owm-run")) == 0);
	CHECK(count_clients() == 1);
	type_command("owm-nonexistent-test-program");
	shortcut(XK_Return, 0);
	CHECK(find_instance("owm-run") == dialog);
	shortcut(XK_a, ControlMask);
	type_command("xterm -name launched -title 'run test'");
	screenshot("run");
	shortcut(XK_Return, 0);
	WAIT(!find_instance("owm-run") && find_instance("launched"));
	message(find_instance("launched"), "_NET_CLOSE_WINDOW", CurrentTime, 0,
		0);
	WAIT(count_clients() == 0);
	shortcut(XK_p, Mod4Mask);
	WAIT(find_instance("owm-run"));

	/*
	 * Each invocation starts empty. Removed history/chooser shortcuts do
	 * not launch the previous command or open another dialog.
	 */
	shortcut(XK_F4, 0);
	shortcut(XK_Down, 0);
	shortcut(XK_b, Mod1Mask);
	CHECK(count_clients() == 1);
	dialog = find_instance("owm-run");
	shortcut(XK_Return, 0); /* Empty OK is disabled. */
	CHECK(find_instance("owm-run") == dialog);

	frame = attributes(parent_of(dialog));

	click(frame.x + frame.width - FRAME_BORDER - 10,
		frame.y + FRAME_BORDER + 10); /* Help caption. */
	shortcut(XK_Escape, 0);
	CHECK(find_instance("owm-run") == dialog);
	save_session();
	shortcut(XK_Escape, 0);
	WAIT(count_clients() == 0);

	stop_wm();
	start_wm();
	CHECK(count_clients() == 0);
	shortcut(XK_p, Mod4Mask);
	WAIT(find_instance("owm-run"));
	shortcut(XK_Tab, 0);
	shortcut(XK_Tab, 0);
	shortcut(XK_Return, 0);
	WAIT(count_clients() == 0);
	stop_wm();
}

static void
run_paste(Window dialog, Window owner, Atom selection, Atom encoding,
	const char *text)
{
	XEvent request, reply = {0};
	XSelectionRequestEvent *event = NULL;
	int attempt;

	XSetSelectionOwner(dpy, selection, owner, CurrentTime);
	XSync(dpy, False);
	CHECK(XGetSelectionOwner(dpy, selection) == owner);
	if (selection == XA_PRIMARY) {
		struct text_font *font = NULL;
		Window child;
		int x, y;
		char name[FONT_NAME_SIZE];

		ui_config_font(name);
		font = font_open(dpy, name);
		CHECK(font);
		CHECK(XTranslateCoordinates(
			dpy, dialog, root, 0, 0, &x, &y, &child));
		window_move_pointer(x + 100,
			y + 34 + 2 * (font->ascent + font->descent + 2));
		font_close(font);
		fake_button(dpy, Button2, True, 0);
		fake_button(dpy, Button2, False, 0);
		XFlush(dpy);
	} else
		shortcut(XK_v, ControlMask);

	for (attempt = 0; attempt < (encoding == XA_STRING ? 2 : 1);
		attempt++) {
		WAIT(XCheckTypedWindowEvent(
			dpy, owner, SelectionRequest, &request));
		event = &request.xselectionrequest;
		CHECK(event->requestor == dialog &&
			event->selection == selection);
		CHECK(event->target ==
			(attempt ? XA_STRING : atom("UTF8_STRING")));
		reply.xselection.type = SelectionNotify;
		reply.xselection.requestor = event->requestor;
		reply.xselection.selection = event->selection;
		reply.xselection.target = event->target;
		reply.xselection.time = event->time;
		reply.xselection.property = None;
		if (event->target == encoding) {
			XChangeProperty(dpy, dialog, event->property, encoding,
				8, PropModeReplace, (const unsigned char *)text,
				(int)strlen(text));
			reply.xselection.property = event->property;
		}
		XSendEvent(dpy, dialog, False, NoEventMask, &reply);
		XFlush(dpy);
	}
	pause_ms(100);
}

static void
check_run_argument(const char *expected)
{
	Window launched;
	int count;
	char **arguments = NULL;

	shortcut(XK_Return, 0);
	WAIT(!find_instance("owm-run") && find_instance("run-utf8"));
	launched = find_instance("run-utf8");
	CHECK(XGetCommand(dpy, launched, &arguments, &count));
	CHECK(count == 4 && strcmp(arguments[3], expected) == 0);
	XFreeStringList(arguments);
	message(launched, "_NET_CLOSE_WINDOW", CurrentTime, 0, 0);
	WAIT(count_clients() == 0);
}

void
test_run_unicode(void)
{
	KeySym *mapping = NULL;
	KeySym keys[] = {XK_aring, 0x01000436, XK_dead_acute};
	XEvent event;
	Window dialog, owner;
	int i, per_key;
	pid_t pid;
	char *locale = NULL, *modifiers = NULL;
	char command[PATH_MAX + 128];
	const char *expected = "\303\245\320\266\303\251";

	/*
	 * Use the local input method so these checks need no desktop IM daemon.
	 */
	if (getenv("LC_ALL") != NULL)
		locale = strdup(getenv("LC_ALL"));
	if (getenv("XMODIFIERS") != NULL)
		modifiers = strdup(getenv("XMODIFIERS"));
	CHECK(setenv("LC_ALL", "C.UTF-8", 1) == 0);
	CHECK(setenv("XMODIFIERS", "@im=none", 1) == 0);
	mapping = XGetKeyboardMapping(dpy, 248, 3, &per_key);
	CHECK(mapping);
	XChangeKeyboardMapping(dpy, 248, 1, keys, 3);
	XSync(dpy, False);
	while (XCheckTypedEvent(dpy, MappingNotify, &event))
		XRefreshKeyboardMapping(&event.xmapping);

	start_wm();
	pid = spawn("./owm-run");
	WAIT(find_instance("owm-run"));
	type_command(fixture_path);
	type_command(" --fixture run-utf8 '");
	for (i = 248; i <= 249; i++) {
		fake_key(dpy, i, True, 0);
		fake_key(dpy, i, False, 0);
	}
	XFlush(dpy);
	/* Editing must remove a whole character, preserving its neighbour. */
	shortcut(XK_Left, 0);
	shortcut(XK_BackSpace, 0);
	fake_key(dpy, 248, True, 0);
	fake_key(dpy, 248, False, 0);
	XFlush(dpy);
	shortcut(XK_End, 0);
	fake_key(dpy, 250, True, 0);
	fake_key(dpy, 250, False, 0);
	XFlush(dpy);
	shortcut(XK_e, 0);
	type_command("'");
	check_run_argument(expected);
	CHECK(wait_exit(pid) == 0);

	owner = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
	for (i = 0; i < 3; i++) {
		pid = spawn("./owm-run");
		WAIT(find_instance("owm-run"));
		dialog = find_instance("owm-run");
		snprintf(command, sizeof(command),
			"'%s' --fixture run-utf8 '%s'", fixture_path,
			i == 1 ? "\345" : expected);
		run_paste(dialog, owner,
			i == 2 ? XA_PRIMARY : atom("CLIPBOARD"),
			i == 1 ? XA_STRING : atom("UTF8_STRING"), command);
		check_run_argument(i == 1 ? "\303\245" : expected);
		CHECK(wait_exit(pid) == 0);
	}
	XDestroyWindow(dpy, owner);
	stop_wm();
	XChangeKeyboardMapping(dpy, 248, per_key, mapping, 3);
	XFree(mapping);
	XSync(dpy, False);
	while (XCheckTypedEvent(dpy, MappingNotify, &event))
		XRefreshKeyboardMapping(&event.xmapping);
	if (locale != NULL)
		CHECK(setenv("LC_ALL", locale, 1) == 0);
	else
		CHECK(unsetenv("LC_ALL") == 0);
	if (modifiers != NULL)
		CHECK(setenv("XMODIFIERS", modifiers, 1) == 0);
	else
		CHECK(unsetenv("XMODIFIERS") == 0);
	free(locale);
	free(modifiers);
}
