#ifndef OWM_TEST_HARNESS_H
#define OWM_TEST_HARNESS_H

#include <sys/stat.h>
#include <sys/wait.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../../src/shared/string-util.h"

/* Expected default frame border, in pixels. */
enum {
	FRAME_BORDER = 6
};

/* Buttons selected by the test click helper, independent of WM internals. */
enum caption_button {
	CAPTION_MINIMIZE,
	CAPTION_MAXIMIZE,
	CAPTION_MENU
};

extern Display *dpy;
extern Window root;
extern pid_t wm_pid;
extern char fixture_path[PATH_MAX], output_path[PATH_MAX];
extern char config_path[PATH_MAX + 32], session_path[PATH_MAX + 32];
extern int (*fake_motion)(Display *, int, int, int, unsigned long);
extern int (*fake_button)(Display *, unsigned int, Bool, unsigned long);
extern int (*fake_key)(Display *, unsigned int, Bool, unsigned long);

#define CHECK(c)                                                               \
	do {                                                                   \
		if (!(c))                                                      \
			fail(#c, __FILE__, __LINE__);                          \
	} while (0)
#define WAIT(c)                                                                \
	do {                                                                   \
		int wait_attempt = 0;                                          \
		while (!(c)) {                                                 \
			if (wait_attempt++ == 150)                             \
				fail(#c, __FILE__, __LINE__);                  \
			pause_ms(20);                                          \
		}                                                              \
	} while (0)

void	harness_init(const char *);
void	cleanup(void);
void	fail(const char *condition, const char *file, int line);
void	pause_ms(int ms);
void	write_config(const char *, ...);
void	check_fullscreen(Window);
int	log_contains(const char *);

int	fixture(int argc, char **argv);
pid_t	spawn(const char *program);
int	wait_exit(pid_t pid);
void	start_wm(void);
void	stop_wm(void);
void	save_session(void);
pid_t	start_xterm(const char *name, const char *text);

Atom			atom(const char *name);
unsigned long	       *property(Window window, const char *name, Atom type,
	unsigned long *n);
unsigned long		value(Window window, const char *name, Atom type);
int			state(Window window, const char *name);
int			count_clients(void);
Window			parent_of(Window window);
XWindowAttributes	attributes(Window window);
int			stack_position(Window window);
int			received_protocol(Window w, const char *protocol);
void			message(Window window, const char *name, long a, long b,
	long c);
Window			make_window(const char *instance, const char *title,
	int override);
Window			find_instance(const char *name);
Window			named_popup(const char *expected);
Window			desktop_menu(void);
Window			taskbar_window(void);
Window			find_icon(const char *id);

void	window_move_pointer(int x, int y);
void	click(int x, int y);
void	right_click(int x, int y);
void	drag(int x, int y, int dx, int dy);
void	shortcut(KeySym key, unsigned int modifiers);
void	type_command(const char *text);
void	caption(Window window, enum caption_button button);
void	close_from_menu(Window window);
void	screenshot(const char *name);

#endif
