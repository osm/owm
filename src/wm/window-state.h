#ifndef OWM_WINDOW_STATE_H
#define OWM_WINDOW_STATE_H

#include <X11/Xutil.h>

#include "geometry.h"

struct client;

/* Value snapshots. Only the window state module changes live placement. */
struct window_state {
	struct geometry	g;
	struct geometry	restore;
	int		minimized;
	int		fullscreen;
	int		max_h;
	int		max_v;
};

void	window_state_snapshot(const struct client *, struct window_state *);
/* For session placement before frame creation. */
void	window_state_seed(struct client *, const struct window_state *);
void	window_state_initialize(struct client *, const XWindowAttributes *,
	struct window_state *);
void	window_state_activate(struct client *, const struct window_state *);
/*
 * Cancel movement without undoing independently requested visibility changes.
 */
void	window_state_restore_move(struct client *, const struct window_state *);
void	window_state_request_fullscreen(struct client *, int);
void	window_state_request_iconify(struct client *);

int	window_client_minimized(const struct client *);
int	window_client_visible(const struct client *);
void	window_client_constrain_size(struct client *, int *, int *);
void	window_client_constrain_move(struct geometry *);
void	window_client_configure(struct client *, struct geometry, int);
void	window_client_sync_visibility(void);
void	window_client_minimize(struct client *, int);
void	window_client_maximize(struct client *, int, int);
void	window_client_fullscreen(struct client *, int);
void	window_client_handle_configure_request(XConfigureRequestEvent *);

#endif
