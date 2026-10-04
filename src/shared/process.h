#ifndef OWM_PROCESS_H
#define OWM_PROCESS_H

#include <sys/types.h>

#include <stddef.h>

/* Caller owns the returned string. NULL reports errno (E2BIG or ENOMEM). */
char	*process_quote_argv(int, char *const[], size_t);

/* Returns after exec, not after the application exits. Failure sets errno. */
pid_t	process_shell(int, const char *);
int	process_launch(int, const char *, char *, size_t);

#endif
