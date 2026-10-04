#ifndef OWM_FONT_H
#define OWM_FONT_H

#include <X11/Xft/Xft.h>

struct text_font {
	Display		*display;
	XftFont		*handle;
	XftDraw		*draw;
	int		ascent;
	int		descent;
	XftColor	colors[8];

	unsigned int	color_count;
	unsigned int	next_color;
};

/* Own the font and its reusable draw context. Close before the display. */
struct text_font	*font_open(Display *, const char *);
void			 font_close(struct text_font *);
/* Release cached drawing resources before destroying their drawable. */
void	font_forget_drawable(struct text_font *, Drawable);
int	font_width(struct text_font *, const char *, int);
void	font_draw(struct text_font *, Window, int, int, const char *, int,
	unsigned long, const XRectangle *);

#endif
