#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "file-replace.h"
#include "memory.h"
#include "window-positions.h"
#include "wm.h"

static struct window_position *positions;
static int writable;
static char path[PATH_MAX + 32];

static struct window_position	**find_position(const struct client *);
static int			  save_positions(void);

void
window_placement_load(void)
{
	FILE *file = NULL;

	snprintf(
		path, sizeof(path), "%s/window-positions", wm.config.directory);
	file = fopen(path, "r");
	writable = 1;
	if (file == NULL) {
		if (errno != ENOENT) {
			warn("%s", path);
			writable = 0;
		}
		return;
	}
	writable = window_positions_read(file, &positions);
	fclose(file);
	if (!writable)
		warnx("%s: invalid window positions", path);
}

void
window_placement_free(void)
{
	window_positions_free(&positions);
}

int
window_placement_eligible(const struct client *c)
{
	return c != NULL && !c->transient && !c->dialog &&
		window_client_is_task(c) &&
		(*c->class_name != '\0' || *c->instance != '\0');
}

static struct window_position **
find_position(const struct client *c)
{
	struct window_position **p = NULL;

	for (p = &positions; *p != NULL; p = &(*p)->next)
		if (strcmp((*p)->class_name, c->class_name) == 0 &&
			strcmp((*p)->instance, c->instance) == 0)
			break;

	return p;
}

int
window_placement_get(const struct client *c, int *x, int *y)
{
	struct window_position *p = NULL;

	if (!window_placement_eligible(c))
		return 0;
	p = *find_position(c);
	if (p == NULL)
		return 0;
	*x = p->x;
	*y = p->y;

	return 1;
}

static int
save_positions(void)
{
	struct file_replace output = {0};

	if (!writable) {
		warnx("%s: refusing to overwrite unreadable positions", path);
		return 0;
	}
	if (!owm_config_make_directory(&wm.config) ||
		!file_replace_open(&output, path)) {
		warn("%s", path);
		return 0;
	}
	if (!window_positions_write(output.stream, positions)) {
		file_replace_abort(&output);
		warnx("%s: cannot write window positions", path);
		return 0;
	}
	if (!file_replace_commit(&output)) {
		warn("%s", path);
		return 0;
	}

	return 1;
}

int
window_placement_save(const struct client *c, int forget)
{
	struct window_position **slot = NULL, *previous = NULL,
			*replacement = NULL;
	struct window_state state;

	if (!window_placement_eligible(c))
		return 0;
	slot = find_position(c);
	previous = *slot;
	if (forget && previous == NULL)
		return 1;
	if (!forget) {
		window_state_snapshot(c, &state);
		replacement = allocate(sizeof(*replacement));
		replacement->class_name = copy_string(c->class_name);
		replacement->instance = copy_string(c->instance);
		replacement->x = state.restore.x;
		replacement->y = state.restore.y;
		replacement->next = previous != NULL ? previous->next : NULL;
	}
	*slot = forget ? previous->next : replacement;
	if (!save_positions()) {
		*slot = previous;
		if (replacement != NULL)
			replacement->next = NULL;
		window_positions_free(&replacement);
		return 0;
	}
	if (previous != NULL)
		previous->next = NULL;
	window_positions_free(&previous);

	return 1;
}
