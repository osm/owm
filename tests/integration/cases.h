#ifndef OWM_TEST_CASES_H
#define OWM_TEST_CASES_H

/* Exit status for a case whose optional dependency is unavailable. */
enum {
	TEST_SKIP = 77
};

void	test_lifecycle(void);
void	test_session_identity(void);
void	test_invalid_session(void);
void	test_restore_child_exit(void);
void	test_signal_shutdown(void);
void	test_xterm_restore(void);
void	test_st_restore(void);
void	test_saved_commands(void);
void	test_desktop_menu(void);
void	test_run_dialog(void);
void	test_run_unicode(void);
void	test_focus_cycle(void);
void	test_transient_stacking(void);
void	test_desktops_and_keys(void);
void	test_shortcuts(void);
void	test_titlebar_clicks(void);
void	test_configurable_menu(void);
void	test_fonts(void);
void	test_window_appearance(void);
void	test_desktop_submenu(void);
void	test_sticky_window(void);
void	test_hidden_desktops(void);
void	test_keyboard_move(void);
void	test_fullscreen(void);
void	test_maximize_transitions(void);
void	test_icons(void);
void	test_taskbar_icons(void);
void	test_desktop_icon_scaling(void);
void	test_restart(void);
void	test_pointer_focus(void);
void	test_tray_tooltip(void);
void	test_taskbar_tooltip(void);
void	test_tray_options(void);
void	test_sdl(void);
void	test_protocol_properties(void);
void	test_metadata_updates(void);

#endif
