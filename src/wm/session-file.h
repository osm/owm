#ifndef OWM_SESSION_FILE_H
#define OWM_SESSION_FILE_H

#include <stdio.h>

#include "geometry.h"

/* Owns its strings and next record. No live clients or restore bookkeeping. */
struct session_record {
	char			*class_name;
	char			*instance;
	char			*title;
	char			*command;
	struct geometry		g;
	int			minimized;
	int			fullscreen;
	int			max_h;
	int			max_v;
	int			order;
	int			desktop;
	struct session_record	*next;
};

/* Caller supplies the default desktop. Failure frees partial records. */
int	session_file_read(FILE *, const char *, int *,
	struct session_record **);
int	session_file_write(FILE *, int, const struct session_record *);
void	session_records_free(struct session_record **);

#endif
