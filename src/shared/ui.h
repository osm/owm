#ifndef OWM_UI_H
#define OWM_UI_H

#include <X11/Xlib.h>

#include "font.h"
#include "ui-colors.h"
#include "ui-palette.h"

struct ui_rect {
	int	x;
	int	y;
	int	width;
	int	height;
};

struct ui {
	Display			*display;
	GC			gc;
	int			clipped;
	int			owns_resources;
	XRectangle		clip;
	struct text_font	*font;
	struct ui_palette	palette;
	const unsigned long	*colors;
};

struct canvas {
	struct ui	*ui;
	Window		window;
};

/*
 * Own the font, GC and palette until ui_free. The display/window are borrowed.
 */
int	ui_init(struct ui *, Display *, Window, const char *);
/* Borrow font, GC and colors until ui_free. The owner keeps them alive. */
void	ui_borrow(struct ui *, Display *, GC, struct text_font *,
	const unsigned long *);
void	ui_clip(struct ui *, const XRectangle *);
void	ui_free(struct ui *);
int	ui_contains(struct ui_rect, int, int);
void	ui_rect(struct canvas, int, int, int, int, unsigned long);
void	ui_label(struct canvas, int, int, const char *, unsigned long);
void	ui_bevel(struct canvas, int, int, int, int, int);
void	ui_button(struct canvas, struct ui_rect, const char *, int, int);

#endif
