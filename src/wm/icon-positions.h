#ifndef OWM_ICON_POSITIONS_H
#define OWM_ICON_POSITIONS_H

struct config;

struct icon_position {
	int	x;
	int	y;
};

/*
 * Borrow config and one position per configured icon. Load updates
 * matching IDs and leaves defaults intact for missing or invalid records.
 * Save replaces the file atomically, returning zero with errno on failure.
 */
void	icon_positions_load(const struct config *, struct icon_position *);
int	icon_positions_save(const struct config *,
	const struct icon_position *);

#endif
