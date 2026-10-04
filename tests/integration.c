#include "integration/cases.h"
#include "integration/harness.h"

static const struct {
	const char	*name;
	void		(*run)(void);
} cases[] = {
	{"lifecycle", test_lifecycle},
	{"session-identity", test_session_identity},
	{"invalid-session", test_invalid_session},
	{"restore-child-exit", test_restore_child_exit},
	{"signal-shutdown", test_signal_shutdown},
	{"xterm-restore", test_xterm_restore},
	{"st-restore", test_st_restore},
	{"saved-commands", test_saved_commands},
	{"desktop-menu", test_desktop_menu},
	{"run-dialog", test_run_dialog},
	{"run-unicode", test_run_unicode},
	{"focus-cycle", test_focus_cycle},
	{"transient-stacking", test_transient_stacking},
	{"desktops-and-keys", test_desktops_and_keys},
	{"shortcuts", test_shortcuts},
	{"titlebar-clicks", test_titlebar_clicks},
	{"configurable-menu", test_configurable_menu},
	{"fonts", test_fonts},
	{"window-appearance", test_window_appearance},
	{"desktop-submenu", test_desktop_submenu},
	{"sticky-window", test_sticky_window},
	{"hidden-desktops", test_hidden_desktops},
	{"keyboard-move", test_keyboard_move},
	{"fullscreen", test_fullscreen},
	{"maximize-transitions", test_maximize_transitions},
	{"icons", test_icons},
	{"taskbar-icons", test_taskbar_icons},
	{"desktop-icon-scaling", test_desktop_icon_scaling},
	{"restart", test_restart},
	{"pointer-focus", test_pointer_focus},
	{"tray-tooltip", test_tray_tooltip},
	{"taskbar-tooltip", test_taskbar_tooltip},
	{"tray-options", test_tray_options},
	{"sdl", test_sdl},
	{"protocol-properties", test_protocol_properties},
	{"metadata-updates", test_metadata_updates},
};

int
main(int argc, char *argv[])
{
	size_t i, index, count;
	int argument, list = 0, reverse, status, selected;
	pid_t pid;
	const char *name = NULL;

	if (argc > 1 && strcmp(argv[1], "--fixture") == 0)
		return fixture(argc, argv);
	reverse = getenv("OWM_TEST_REVERSE") != NULL;
	for (argument = 1; argument < argc; argument++) {
		if (strcmp(argv[argument], "--reverse") == 0)
			reverse = 1;
		else if (strcmp(argv[argument], "--list") == 0)
			list = 1;
		else if (strcmp(argv[argument], "--case") == 0 &&
			argument + 1 < argc && argv[argument + 1][0] != '\0' &&
			strncmp(argv[argument + 1], "--", 2) != 0)
			name = argv[++argument];
		else {
			fprintf(stderr,
				"Usage: %s [--list] [--case NAME] "
				"[--reverse]\n",
				argv[0]);
			return 1;
		}
	}
	count = sizeof(cases) / sizeof(*cases);
	selected = 0;
	for (i = 0; i < count; i++) {
		index = reverse ? count - 1 - i : i;
		if (name != NULL && strcmp(name, cases[index].name))
			continue;
		selected = 1;
		if (list) {
			puts(cases[index].name);
			continue;
		}
		printf("CASE %s\n", cases[index].name);
		fflush(NULL);
		pid = fork();
		if (pid < 0) {
			perror("fork test case");
			return 1;
		}
		if (pid == 0) {
			harness_init(cases[index].name);
			cases[index].run();
			CHECK(wm_pid == 0);
			cleanup();
			exit(0);
		}
		while (waitpid(pid, &status, 0) < 0) {
			if (errno == EINTR)
				continue;
			perror("wait test case");
			return 1;
		}
		if (!WIFEXITED(status))
			return 1;
		if (WEXITSTATUS(status) == TEST_SKIP)
			printf("SKIP %s\n", cases[index].name);
		else if (WEXITSTATUS(status) == 0)
			printf("PASS %s\n", cases[index].name);
		else
			return 1;
	}
	if (!selected)
		fprintf(stderr, "Unknown test case: %s\n", name);

	return selected ? 0 : 1;
}
