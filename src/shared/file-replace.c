#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "file-replace.h"

int
file_replace_open(struct file_replace *file, const char *destination)
{
	int error, fd, length;

	file->stream = NULL;
	file->destination = destination;
	length = snprintf(file->temporary, sizeof(file->temporary),
		"%s.tmp.XXXXXX", destination);
	if (length < 0 || (size_t)length >= sizeof(file->temporary)) {
		file->temporary[0] = '\0';
		errno = ENAMETOOLONG;
		return 0;
	}

	fd = mkstemp(file->temporary);
	if (fd < 0) {
		file->temporary[0] = '\0';
		return 0;
	}

	file->stream = fdopen(fd, "w");
	if (file->stream != NULL)
		return 1;

	error = errno;
	close(fd);
	file_replace_abort(file);
	errno = error;

	return 0;
}

void
file_replace_abort(struct file_replace *file)
{
	int error;

	error = errno;
	if (file->stream != NULL)
		fclose(file->stream);
	if (*file->temporary != '\0')
		unlink(file->temporary);
	file->stream = NULL;
	file->destination = NULL;
	file->temporary[0] = '\0';
	errno = error;
}

int
file_replace_commit(struct file_replace *file)
{
	int error;

	error = ferror(file->stream) ? EIO : 0;
	if (fflush(file->stream) != 0 && error == 0)
		error = errno;
	if (error == 0 && fsync(fileno(file->stream)) != 0)
		error = errno;
	if (fclose(file->stream) != 0 && error == 0)
		error = errno;
	file->stream = NULL;

	if (error == 0 && rename(file->temporary, file->destination) != 0)
		error = errno;
	if (error != 0) {
		file_replace_abort(file);
		errno = error;
		return 0;
	}

	file->destination = NULL;
	file->temporary[0] = '\0';

	return 1;
}
