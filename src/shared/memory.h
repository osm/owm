#ifndef OWM_MEMORY_H
#define OWM_MEMORY_H

#include <stddef.h>

/* Return owned, zeroed storage. Allocation failure terminates the process. */
void	*allocate(size_t);
void	*allocate_array(size_t, size_t);

/* Return an owned copy. NULL input produces an empty string. */
char	*copy_string(const char *);

#endif
