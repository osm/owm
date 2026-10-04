#ifndef OWM_IMAGE_H
#define OWM_IMAGE_H

#include <X11/Xlib.h>

/* Initialize to zero. Owns pixmap, optional mask, and allocated XPM colors. */
struct wm_image {
	Pixmap		pixmap;
	Pixmap		mask;
	unsigned int	width;
	unsigned int	height;
	unsigned long	*pixels;
	int		pixel_count;
};

/* Replaces existing contents. Free before closing the WM display. */
int	wm_image_load(struct wm_image *, const char *, unsigned long,
	unsigned int);
/* Resizes artwork and mask atomically. Failure leaves the image unchanged. */
int	wm_image_resize(struct wm_image *, unsigned int, unsigned int);
void	wm_image_free(struct wm_image *);

/* These return an owned pixmap. Release with XFreePixmap. */
Pixmap	wm_image_from_rgba(const unsigned char *, unsigned int, unsigned int,
	unsigned long, unsigned int, unsigned int *, unsigned int *, int);
Pixmap	wm_image_load_file(const char *, unsigned long, unsigned int,
	unsigned int *, unsigned int *, int);

#endif
