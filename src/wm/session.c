#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "clock.h"
#include "file-replace.h"
#include "memory.h"
#include "process.h"
#include "session-file.h"
#include "wm.h"

#define RESTORE_TIMEOUT_SECONDS 30

struct session_entry {
	struct session_record	*record;
	int			matched;
	int			expired;
	pid_t			launch_pid;
	struct session_entry	*next;
};

static struct session_entry *records = NULL;
static double restore_deadline;

static int			 read_entries(FILE *, int *,
	struct session_entry **);
static int			 read_saved_commands(struct session_entry **);
static void			 free_entries(struct session_entry **);
static struct session_entry	*match_record(const struct client *,
	struct session_entry *);
static struct session_record	*snapshot_client(struct client *, int,
	struct session_entry *);

static int
read_entries(FILE *file, int *desktop, struct session_entry **head)
{
	struct session_record *data = NULL;
	struct session_entry **tail = NULL;

	*desktop = wm.current_desktop + 1;
	if (!session_file_read(file, wm.config.session, desktop, &data))
		return 0;
	tail = head;
	while (*tail != NULL)
		tail = &(*tail)->next;
	while (data != NULL) {
		struct session_entry *entry = NULL;

		entry = allocate(sizeof(*entry));
		entry->record = data;
		data = data->next;
		entry->record->next = NULL;
		*tail = entry;
		tail = &entry->next;
	}

	return 1;
}

static void
free_entries(struct session_entry **head)
{
	while (*head != NULL) {
		struct session_entry *entry = NULL;

		entry = *head;
		*head = entry->next;
		session_records_free(&entry->record);
		free(entry);
	}
}

void
owm_session_free(void)
{
	free_entries(&records);
}

void
owm_session_load(void)
{
	FILE *file = NULL;
	struct session_entry *r = NULL;
	int ok, saved_desktop;

	file = fopen(wm.config.session, "r");

	if (file == NULL) {
		if (errno != ENOENT)
			warn("%s", wm.config.session);
		return;
	}

	ok = read_entries(file, &saved_desktop, &records);
	fclose(file);

	if (!ok)
		return;

	wm.current_desktop = saved_desktop <= wm.config.desktops
		? saved_desktop - 1
		: wm.config.desktops - 1;

	for (r = records; r != NULL; r = r->next)
		if (r->record->order >= wm.next_order)
			wm.next_order = r->record->order + 1;
}

static struct session_record *
snapshot_client(struct client *c, int order, struct session_entry *saved)
{
	struct session_record *record = NULL;
	struct session_entry *previous = NULL;

	/*
	 * Dialogs are recreated by their parent, not launched as
	 * independent apps.
	 */
	if (!window_client_is_task(c) || c->transient)
		return NULL;

	window_client_refresh(c);
	previous = match_record(c, saved);
	if (previous != NULL) {
		previous->matched = 1;
		if ((c->command == NULL || *c->command == '\0') &&
			*previous->record->command != '\0') {
			free(c->command);
			c->command = copy_string(previous->record->command);
		}
	}

	if (*c->class_name == '\0' && *c->instance == '\0') {
		warnx("cannot save '%s': no WM_CLASS identity", c->title);
		return NULL;
	}

	record = allocate(sizeof(*record));
	record->g = c->fullscreen || c->max_h || c->max_v ? c->restore : c->g;
	record->class_name = copy_string(c->class_name);
	record->instance = copy_string(c->instance);
	record->title = copy_string(c->title);
	record->command = copy_string(c->command);
	record->minimized = c->minimized;
	record->fullscreen = c->fullscreen;
	record->max_h = c->max_h;
	record->max_v = c->max_v;
	record->desktop = desktop_for_client(c);
	record->order = order;

	if (*record->command == '\0')
		warnx("saved '%s' without a launch command (set command in "
		"session to enable relaunch)",
			record->title);

	return record;
}

/* A saved command may be the only launch information a client provides. */
static int
read_saved_commands(struct session_entry **saved)
{
	FILE *file = NULL;
	struct client *c = NULL;
	int desktop, ok;

	for (c = wm.clients; c != NULL; c = c->next)
		if (window_client_is_task(c) && !c->transient &&
			(c->command == NULL || *c->command == '\0'))
			break;
	if (c == NULL)
		return 1;

	file = fopen(wm.config.session, "r");
	if (file == NULL) {
		if (errno == ENOENT)
			return 1;
		warn("cannot read saved commands from %s", wm.config.session);
		return 0;
	}
	ok = read_entries(file, &desktop, saved);
	fclose(file);

	return ok;
}

int
owm_session_save(void)
{
	struct client *c = NULL;
	struct session_entry *saved = NULL;
	struct file_replace output = {0};
	struct session_record *snapshot = NULL, **tail = NULL;
	int directory, order;

	if (!owm_config_make_directory(&wm.config))
		return 0;

	if (!read_saved_commands(&saved))
		return 0;

	/* Resolve metadata and saved commands before any serialization. */
	order = 0;
	tail = &snapshot;
	for (c = wm.clients; c != NULL; c = c->next) {
		*tail = snapshot_client(c, order, saved);
		if (*tail != NULL) {
			order++;
			tail = &(*tail)->next;
		}
	}
	free_entries(&saved);
	if (!file_replace_open(&output, wm.config.session)) {
		session_records_free(&snapshot);
		warn("cannot create session temporary file");
		return 0;
	}
	if (!session_file_write(
		output.stream, wm.current_desktop + 1, snapshot)) {
		session_records_free(&snapshot);
		file_replace_abort(&output);
		warn("session write failed");
		return 0;
	}
	session_records_free(&snapshot);

	if (!file_replace_commit(&output)) {
		warn("session save failed");
		return 0;
	}

	directory = open(wm.config.directory, O_RDONLY | O_DIRECTORY);

	if (directory >= 0) {
		if (fsync(directory) < 0)
			warn("session directory sync");
		close(directory);
	}

	fprintf(stderr, "owm: saved %d window%s to %s\n", order,
		order == 1 ? "" : "s", wm.config.session);

	return 1;
}

static struct session_entry *
match_record(const struct client *c, struct session_entry *head)
{
	struct session_entry *best = NULL, *r = NULL;
	int best_score;

	best_score = 0;

	for (r = head; r != NULL; r = r->next) {
		int same_class, same_instance, score;

		if (r->matched || r->expired)
			continue;

		same_class =
			(strcmp(r->record->class_name, c->class_name) == 0);
		same_instance = (strcmp(r->record->instance, c->instance) == 0);
		score = 0;

		if (same_class && same_instance)
			score = 100;

		/*
		 * Never let a differently named terminal steal a named
		 * terminal's slot.
		 */
		else if (same_class &&
			(*r->record->instance == '\0' || *c->instance == '\0'))
			score = 50;
		else if (same_instance && *r->record->instance != '\0' &&
			(*r->record->class_name == '\0' ||
				*c->class_name == '\0'))
			score = 40;

		if (score && strcmp(r->record->title, c->title) == 0)
			score += 10;

		if (score > best_score) {
			best = r;
			best_score = score;
		}
	}

	return best;
}

int
owm_session_match(struct client *c)
{
	struct session_entry *best = NULL;
	struct window_state state;

	if (c->session_claimed || !window_client_is_task(c) || c->transient)
		return 0;
	best = match_record(c, records);

	if (best == NULL)
		return 0;

	best->matched = 1;
	c->session_claimed = 1;
	state.g = state.restore = best->record->g;
	state.minimized = best->record->minimized;
	state.fullscreen = best->record->fullscreen;
	state.max_h = best->record->max_h;
	state.max_v = best->record->max_v;
	window_state_seed(c, &state);
	c->order = best->record->order;
	c->desktop = best->record->desktop < wm.config.desktops
		? best->record->desktop
		: wm.config.desktops - 1;

	if (c->command == NULL ||
		(*c->command == '\0' && *best->record->command != '\0')) {
		free(c->command);
		c->command = copy_string(best->record->command);
	}

	return 1;
}

void
owm_session_launch(void)
{
	struct session_entry *r = NULL;

	restore_deadline = records != NULL
		? monotonic_time() + RESTORE_TIMEOUT_SECONDS
		: 0;
	XFlush(wm.display);

	for (r = records; r != NULL; r = r->next) {
		pid_t pid;

		if (r->matched || r->expired)
			continue;

		if (*r->record->command == '\0') {
			warnx("no launch command for %s/%s; waiting for a "
			"matching "
			"window",
				r->record->class_name, r->record->instance);
			continue;
		}

		pid = process_shell(
			ConnectionNumber(wm.display), r->record->command);

		if (pid < 0) {
			warn("cannot launch %s/%s", r->record->class_name,
				r->record->instance);
			continue;
		}

		r->launch_pid = pid;
	}
}

void
owm_session_reaped(pid_t pid, int status)
{
	struct session_entry *r = NULL;

	for (r = records; r != NULL; r = r->next) {
		if (r->launch_pid != pid)
			continue;

		r->launch_pid = 0;

		if ((WIFEXITED(status) && WEXITSTATUS(status)) ||
			WIFSIGNALED(status)) {
			warnx("restore process for %s/%s exited %s %d",
				r->record->class_name, r->record->instance,
				WIFEXITED(status) ? "with status" : "on signal",
				WIFEXITED(status) ? WEXITSTATUS(status)
						: WTERMSIG(status));
		}
		break;
	}
}

double
owm_session_deadline(void)
{
	return restore_deadline;
}

void
owm_session_tick(double now)
{
	struct session_entry *r = NULL;

	if (restore_deadline <= 0 || now < restore_deadline)
		return;

	restore_deadline = 0;

	for (r = records; r != NULL; r = r->next) {
		if (r->matched || r->expired)
			continue;

		warnx("restore timed out for %s/%s ('%s')",
			r->record->class_name, r->record->instance,
			r->record->title);
		r->expired = 1;
	}
}
