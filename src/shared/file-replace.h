#ifndef OWM_FILE_REPLACE_H
#define OWM_FILE_REPLACE_H

#include <limits.h>
#include <stdio.h>

struct file_replace {
	FILE		*stream;
	const char	*destination;
	char		temporary[PATH_MAX + 32];
};

/*
 * Own the temporary stream and file. Borrow destination until commit/abort.
 * Commit flushes, syncs and closes before renaming. Failure preserves the
 * destination and sets errno. Both commit and abort release the temporary.
 */
int	file_replace_open(struct file_replace *, const char *);
int	file_replace_commit(struct file_replace *);
void	file_replace_abort(struct file_replace *);

#endif
