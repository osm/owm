#ifndef OWM_TEXT_INPUT_H
#define OWM_TEXT_INPUT_H

#include <stddef.h>

#include "ui.h"

enum {
	TEXT_INPUT_SIZE = 2048
};

struct text_input {
	char	text[TEXT_INPUT_SIZE];
	size_t	cursor;
	size_t	offset;
	int	selected;
};

int	text_input_insert(struct text_input *, const char *, size_t);
void	text_input_draw(struct canvas, struct ui_rect, struct text_input *,
	int);
void	text_input_click(struct text_input *, struct text_font *,
	struct ui_rect, int);
int	text_input_key(struct text_input *, KeySym, unsigned int, const char *,
	int);

#endif
