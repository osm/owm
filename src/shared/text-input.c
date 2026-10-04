#include <X11/keysym.h>
#include <string.h>

#include "text-input.h"
#include "utf8.h"

int
text_input_insert(struct text_input *entry, const char *text, size_t length)
{
	size_t current;

	current = entry->selected ? 0 : strlen(entry->text);

	if (length > sizeof(entry->text) - current - 1)
		return 0;

	if (entry->selected) {
		entry->text[0] = '\0';
		entry->cursor = entry->offset = 0;
		entry->selected = 0;
	}

	memmove(entry->text + entry->cursor + length,
		entry->text + entry->cursor, current - entry->cursor + 1);
	memcpy(entry->text + entry->cursor, text, length);
	entry->cursor += length;

	return 1;
}

void
text_input_draw(struct canvas canvas, struct ui_rect bounds,
	struct text_input *entry, int focused)
{
	struct ui *ui = NULL;
	size_t end, length;
	XRectangle clip = {0};
	int text_width, text_x, text_y;

	ui = canvas.ui;
	text_x = bounds.x + 4;
	text_y = bounds.y + 3;
	text_width = bounds.width - 11;
	length = strlen(entry->text);

	if (entry->offset > length)
		entry->offset = length;

	entry->offset = utf8_floor(entry->text, entry->offset);

	if (entry->cursor < entry->offset)
		entry->offset = entry->cursor;

	while (entry->offset < entry->cursor &&
		font_width(ui->font, entry->text + entry->offset,
			(int)(entry->cursor - entry->offset)) > text_width)
		entry->offset = utf8_next(entry->text, entry->offset);

	end = entry->offset;

	while (entry->text[end] != '\0') {
		size_t next;

		next = utf8_next(entry->text, end);

		if (font_width(ui->font, entry->text + entry->offset,
			(int)(next - entry->offset)) > text_width)
			break;

		end = next;
	}

	clip.x = (short)text_x;
	clip.y = (short)text_y;
	clip.width = (unsigned short)(bounds.width - 6);
	clip.height = (unsigned short)(bounds.height - 6);

	ui_clip(ui, &clip);

	if (entry->selected)
		ui_rect(canvas, text_x, text_y,
			font_width(ui->font, entry->text + entry->offset,
				(int)(end - entry->offset)),
			bounds.height - 6, ui->colors[ACTIVE]);
	XSetForeground(ui->display, ui->gc,
		entry->selected ? ui->colors[WHITE] : ui->colors[BLACK]);
	font_draw(ui->font, canvas.window, text_x,
		bounds.y +
			(bounds.height - ui->font->ascent - ui->font->descent) /
				2 +
			ui->font->ascent,
		entry->text + entry->offset, (int)(end - entry->offset),
		entry->selected ? ui->colors[WHITE] : ui->colors[BLACK], &clip);

	if (focused && !entry->selected)
		ui_rect(canvas,
			text_x +
				font_width(ui->font,
					entry->text + entry->offset,
					(int)(entry->cursor - entry->offset)),
			text_y, 1, bounds.height - 7, ui->colors[BLACK]);
	ui_clip(ui, NULL);
}

void
text_input_click(struct text_input *entry, struct text_font *font,
	struct ui_rect bounds, int x)
{
	entry->selected = 0;
	entry->cursor = entry->offset;

	while (entry->text[entry->cursor] != '\0') {
		size_t next;

		next = utf8_next(entry->text, entry->cursor);

		if (font_width(font, entry->text + entry->offset,
			(int)(next - entry->offset)) >= x - bounds.x - 4)
			break;

		entry->cursor = next;
	}
}

int
text_input_key(struct text_input *entry, KeySym key, unsigned int state,
	const char *text, int length)
{
	int control;

	control = state & ControlMask;

	if (control && (key == XK_a || key == XK_A)) {
		entry->selected = 1;
		return 1;
	}

	switch (key) {
	case XK_Home:
		entry->cursor = 0;
		entry->selected = 0;
		return 1;

	case XK_End:
		entry->cursor = strlen(entry->text);
		entry->selected = 0;
		return 1;

	case XK_Left:
		if (entry->cursor)
			entry->cursor =
				utf8_previous(entry->text, entry->cursor);
		entry->selected = 0;
		return 1;

	case XK_Right:
		if (entry->text[entry->cursor] != '\0')
			entry->cursor = utf8_next(entry->text, entry->cursor);
		entry->selected = 0;
		return 1;

	case XK_BackSpace:
	case XK_Delete:
		if (entry->selected) {
			entry->text[0] = '\0';
			entry->cursor = entry->offset = 0;
			entry->selected = 0;
			return 1;
		}

		if (key == XK_BackSpace && entry->cursor) {
			size_t previous;

			previous = utf8_previous(entry->text, entry->cursor);

			memmove(entry->text + previous,
				entry->text + entry->cursor,
				strlen(entry->text + entry->cursor) + 1);
			entry->cursor = previous;

			return 1;
		}

		if (key == XK_Delete && entry->text[entry->cursor] != '\0') {
			size_t next;

			next = utf8_next(entry->text, entry->cursor);

			memmove(entry->text + entry->cursor, entry->text + next,
				strlen(entry->text + next) + 1);
		}
		return 1;
	default:
		break;
	}

	if (length > 0 && !control && (state & Mod1Mask) == 0 &&
		(unsigned char)text[0] >= 32) {
		return text_input_insert(entry, text, (size_t)length);
	}

	return 1;
}
