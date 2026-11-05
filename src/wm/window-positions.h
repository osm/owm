#ifndef OWM_WINDOW_POSITIONS_H
#define OWM_WINDOW_POSITIONS_H

#include <stdio.h>

/* Owned records, independent of live windows. */
struct window_position {
	char			*class_name;
	char			*instance;
	int			x;
	int			y;
	struct window_position	*next;
};

/* Read into an empty list. Failure frees partial records. */
int	window_positions_read(FILE *, struct window_position **);
int	window_positions_write(FILE *, const struct window_position *);
void	window_positions_free(struct window_position **);

#endif
