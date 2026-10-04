#include <sys/stat.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "paths.h"
#include "string-util.h"

int
config_directory(char *out, size_t size)
{
	struct stat st;
	int n;
	char base[PATH_MAX];
	const char *home = NULL, *xdg = NULL;

	xdg = getenv("XDG_CONFIG_HOME");
	home = getenv("HOME");

	if (xdg != NULL && *xdg == '/') {
		if (string_copy(base, xdg, sizeof(base)) >= sizeof(base)) {
			errno = ENAMETOOLONG;
			return 0;
		}
	} else if (home != NULL && *home != '\0') {
		n = snprintf(base, sizeof(base), "%s/.config", home);
		if (n < 0 || (size_t)n >= sizeof(base)) {
			errno = ENAMETOOLONG;
			return 0;
		}
	} else {
		errno = ENOENT;
		return 0;
	}

	n = snprintf(out, size, "%s/owm", base);

	if (n < 0 || (size_t)n >= size) {
		errno = ENAMETOOLONG;
		return 0;
	}

	if (stat(out, &st) == 0) {
		if (S_ISDIR(st.st_mode))
			return 1;

		errno = ENOTDIR;

		return 0;
	}

	if (errno != ENOENT)
		return 0;

	return 1;
}

int
directory_create(const char *directory)
{
	char *p = NULL;
	char path[PATH_MAX];

	if (*directory == '\0') {
		errno = ENOENT;
		return 0;
	}

	if (strlen(directory) >= sizeof(path)) {
		errno = ENAMETOOLONG;
		return 0;
	}

	string_copy(path, directory, sizeof(path));

	for (p = path + 1;; p++) {
		struct stat st;
		char saved;

		if (*p != '/' && *p != '\0')
			continue;

		saved = *p;

		*p = '\0';

		if (mkdir(path, 0700) < 0 && errno != EEXIST)
			return 0;

		if (stat(path, &st))
			return 0;

		if (!S_ISDIR(st.st_mode)) {
			errno = ENOTDIR;
			return 0;
		}

		*p = saved;

		if (saved == '\0')
			return 1;
	}
}
