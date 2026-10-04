#include <X11/xpm.h>
#include <png.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "memory.h"
#include "wm.h"

#define MAX_XPM_DIMENSION 48
#define MAX_XPM_COLORS 256

/*
 * Cache composited pixmaps so ordinary redraws do not decode or blend icons.
 */
static unsigned long	channel_pixel(unsigned int, unsigned long);
static Pixmap		load_png(const char *, unsigned long, unsigned int,
	unsigned int *, unsigned int *, int);
static int		decode_xpm(const XpmImage *, unsigned char *);
static Pixmap		load_xpm(const char *, unsigned long, unsigned int,
	unsigned int *, unsigned int *, int);
static Pixmap		scale_pixmap(Pixmap, unsigned int, unsigned int,
	unsigned int, unsigned int);

static unsigned long
channel_pixel(unsigned int value, unsigned long mask)
{
	unsigned int shift = 0;

	while (mask && (mask & 1) == 0) {
		mask >>= 1;
		shift++;
	}

	return ((value * mask + 127) / 255) << shift;
}

static Pixmap
load_png(const char *path, unsigned long background_pixel, unsigned int limit,
	unsigned int *out_width, unsigned int *out_height, int enlarge)
{
	png_image source = {0};
	Visual *visual = NULL;
	Pixmap result;
	unsigned char *pixels = NULL;

	visual = DefaultVisual(wm.display, wm.screen);

	if (visual->class != TrueColor)
		return None;

	source.version = PNG_IMAGE_VERSION;

	if (png_image_begin_read_from_file(&source, path) == 0) {
		png_image_free(&source);
		return None;
	}

	if (source.width == 0 || source.height == 0 ||
		source.width > MAX_PNG_DIMENSION ||
		source.height > MAX_PNG_DIMENSION) {
		png_image_free(&source);
		return None;
	}

	source.format = PNG_FORMAT_RGBA;

	pixels = allocate_array(source.height, (size_t)source.width * 4);

	if (png_image_finish_read(&source, NULL, pixels, 0, NULL) == 0) {
		free(pixels);
		png_image_free(&source);
		return None;
	}

	result = wm_image_from_rgba(pixels, source.width, source.height,
		background_pixel, limit, out_width, out_height, enlarge);

	free(pixels);
	png_image_free(&source);

	return result;
}

Pixmap
wm_image_from_rgba(const unsigned char *pixels, unsigned int source_width,
	unsigned int source_height, unsigned long background_pixel,
	unsigned int limit, unsigned int *out_width, unsigned int *out_height,
	int enlarge)
{
	XImage *image = NULL;
	Visual *visual = NULL;
	XColor background = {0};
	Pixmap pixmap;
	unsigned int bg[3] = {0};
	unsigned int height, width, y;

	visual = DefaultVisual(wm.display, wm.screen);

	if (visual->class != TrueColor || source_width == 0 ||
		source_height == 0 || limit == 0)
		return None;

	width = source_width;
	height = source_height;

	if (enlarge || width > limit || height > limit) {
		unsigned int largest;

		largest = width > height ? width : height;
		width = width * limit / largest;
		height = height * limit / largest;

		if (width == 0)
			width = 1;

		if (height == 0)
			height = 1;
	}

	image = XCreateImage(wm.display, visual,
		DefaultDepth(wm.display, wm.screen), ZPixmap, 0, NULL, width,
		height, 32, 0);

	if (image == NULL)
		return None;

	image->data = allocate_array(height, (size_t)image->bytes_per_line);

	background.pixel = background_pixel;

	XQueryColor(wm.display, DefaultColormap(wm.display, wm.screen),
		&background);
	bg[0] = background.red / 257;
	bg[1] = background.green / 257;
	bg[2] = background.blue / 257;

	for (y = 0; y < height; y++) {
		unsigned int x;

		for (x = 0; x < width; x++) {
			int c;
			unsigned int alpha, rgb[3];
			const unsigned char *rgba = NULL;

			rgba = pixels +
				4 *
					((y * source_height / height) *
							source_width +
						x * source_width / width);
			alpha = rgba[3];

			for (c = 0; c < 3; c++)
				rgb[c] = (rgba[c] * alpha +
						bg[c] * (255 - alpha) + 127) /
					255;

			XPutPixel(image, x, y,
				channel_pixel(rgb[0], visual->red_mask) |
					channel_pixel(
						rgb[1], visual->green_mask) |
					channel_pixel(
						rgb[2], visual->blue_mask));
		}
	}

	pixmap = XCreatePixmap(wm.display, wm.root, width, height,
		DefaultDepth(wm.display, wm.screen));

	XPutImage(wm.display, pixmap, wm.gc, image, 0, 0, 0, 0, width, height);
	XDestroyImage(image);
	*out_width = width;
	*out_height = height;

	return pixmap;
}

/* The caller supplies room for MAX_XPM_DIMENSION squared RGBA pixels. */
static int
decode_xpm(const XpmImage *source, unsigned char *pixels)
{
	unsigned int i;
	unsigned char colors[MAX_XPM_COLORS][4] = {{0}};

	if (source->width == 0 || source->height == 0 ||
		source->width > MAX_XPM_DIMENSION ||
		source->height > MAX_XPM_DIMENSION ||
		source->ncolors > MAX_XPM_COLORS)
		return 0;

	for (i = 0; i < source->ncolors; i++) {
		XColor color;
		const char *name = NULL;

		name = source->colorTable[i].c_color;

		if (name == NULL)
			name = source->colorTable[i].g_color;

		if (name == NULL)
			name = source->colorTable[i].m_color;

		if (name == NULL)
			return 0;

		if (strcasecmp(name, "None") == 0)
			continue;

		if (XParseColor(wm.display,
			DefaultColormap(wm.display, wm.screen), name,
			&color) == 0)
			return 0;

		colors[i][0] = color.red / 257;
		colors[i][1] = color.green / 257;
		colors[i][2] = color.blue / 257;
		colors[i][3] = 255;
	}

	for (i = 0; i < source->width * source->height; i++) {
		if (source->data[i] >= source->ncolors)
			return 0;

		memcpy(pixels + i * 4, colors[source->data[i]], 4);
	}

	return 1;
}

static Pixmap
load_xpm(const char *path, unsigned long background_pixel, unsigned int limit,
	unsigned int *out_width, unsigned int *out_height, int enlarge)
{
	XpmImage source = {0};
	Pixmap result = None;
	unsigned char pixels[MAX_XPM_DIMENSION * MAX_XPM_DIMENSION * 4];

	if (XpmReadFileToXpmImage(path, &source, NULL) != XpmSuccess)
		return None;

	if (decode_xpm(&source, pixels))
		result = wm_image_from_rgba(pixels, source.width, source.height,
			background_pixel, limit, out_width, out_height,
			enlarge);

	XpmFreeXpmImage(&source);

	return result;
}

Pixmap
wm_image_load_file(const char *path, unsigned long background_pixel,
	unsigned int limit, unsigned int *out_width, unsigned int *out_height,
	int enlarge)
{
	const char *extension = NULL;

	extension = strrchr(path, '.');

	if (extension != NULL && strcasecmp(extension, ".xpm") == 0)
		return load_xpm(path, background_pixel, limit, out_width,
			out_height, enlarge);

	return load_png(
		path, background_pixel, limit, out_width, out_height, enlarge);
}

static Pixmap
scale_pixmap(Pixmap source, unsigned int width, unsigned int height,
	unsigned int target_width, unsigned int target_height)
{
	XImage *input = NULL, *output = NULL;
	GC gc = NULL;
	Pixmap result;
	unsigned int depth, y;

	input = XGetImage(
		wm.display, source, 0, 0, width, height, AllPlanes, ZPixmap);

	if (input == NULL)
		return None;

	depth = (unsigned int)input->depth;

	output = XCreateImage(wm.display, DefaultVisual(wm.display, wm.screen),
		depth, ZPixmap, 0, NULL, target_width, target_height, 32, 0);

	if (output == NULL) {
		XDestroyImage(input);
		return None;
	}

	output->data =
		allocate_array(target_height, (size_t)output->bytes_per_line);

	for (y = 0; y < target_height; y++) {
		unsigned int x;

		for (x = 0; x < target_width; x++)
			XPutPixel(output, x, y,
				XGetPixel(input, x * width / target_width,
					y * height / target_height));
	}

	result = XCreatePixmap(
		wm.display, wm.root, target_width, target_height, depth);
	gc = XCreateGC(wm.display, result, 0, NULL);

	XPutImage(wm.display, result, gc, output, 0, 0, 0, 0, target_width,
		target_height);
	XFreeGC(wm.display, gc);
	XDestroyImage(output);
	XDestroyImage(input);

	return result;
}

void
wm_image_free(struct wm_image *image)
{
	if (image->pixmap)
		XFreePixmap(wm.display, image->pixmap);
	if (image->mask)
		XFreePixmap(wm.display, image->mask);
	if (image->pixel_count)
		XFreeColors(wm.display, DefaultColormap(wm.display, wm.screen),
			image->pixels, image->pixel_count, 0);
	if (image->pixels != NULL)
		XFree(image->pixels);
	memset(image, 0, sizeof(*image));
}

int
wm_image_load(struct wm_image *image, const char *path,
	unsigned long background, unsigned int limit)
{
	XpmAttributes attributes = {0};
	const char *extension = NULL;
	int status;

	wm_image_free(image);
	extension = strrchr(path, '.');
	if (extension != NULL && strcasecmp(extension, ".png") == 0) {
		image->pixmap = load_png(path, background, limit, &image->width,
			&image->height, 0);
		return image->pixmap != None;
	}
	attributes.valuemask = XpmSize | XpmCloseness | XpmReturnAllocPixels;
	attributes.closeness = 40000;
	status = XpmReadFileToPixmap(wm.display, wm.root, path, &image->pixmap,
		&image->mask, &attributes);

	if (status >= XpmSuccess && attributes.width <= limit &&
		attributes.height <= limit) {
		image->pixels = attributes.alloc_pixels;
		image->pixel_count = attributes.nalloc_pixels;
		attributes.alloc_pixels = NULL;
		attributes.nalloc_pixels = 0;
		image->width = attributes.width;
		image->height = attributes.height;
	} else {

		if (image->pixmap)
			XFreePixmap(wm.display, image->pixmap);

		if (image->mask)
			XFreePixmap(wm.display, image->mask);
		image->pixmap = image->mask = None;
	}

	if (attributes.nalloc_pixels)
		XFreeColors(wm.display, DefaultColormap(wm.display, wm.screen),
			attributes.alloc_pixels, attributes.nalloc_pixels, 0);
	XpmFreeAttributes(&attributes);

	return image->pixmap != None;
}

int
wm_image_resize(struct wm_image *resource, unsigned int w, unsigned int h)
{
	Pixmap image, mask;

	if (resource->pixmap == None || w == 0 || h == 0)
		return 0;
	image = scale_pixmap(
		resource->pixmap, resource->width, resource->height, w, h);
	mask = resource->mask ? scale_pixmap(resource->mask, resource->width,
					resource->height, w, h)
			: None;

	if (image == 0 || (resource->mask && mask == 0)) {
		if (image)
			XFreePixmap(wm.display, image);
		if (mask)
			XFreePixmap(wm.display, mask);
		return 0;
	}

	XFreePixmap(wm.display, resource->pixmap);
	if (resource->mask)
		XFreePixmap(wm.display, resource->mask);
	resource->pixmap = image;
	resource->mask = mask;
	resource->width = w;
	resource->height = h;

	return 1;
}
