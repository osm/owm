#include "wm.h"

int
desktop_work_height(void)
{
	return wm.height - wm.config.taskbar_height;
}

const struct client *
desktop_owner(const struct client *c)
{
	struct client *ancestor = NULL;

	ancestor = window_client_ancestor(c);

	return ancestor != NULL ? ancestor : c;
}

int
desktop_for_client(const struct client *c)
{
	c = desktop_owner(c);

	return (c != NULL) ? c->desktop : wm.current_desktop;
}

int
desktop_contains(const struct client *c)
{
	int desktop;

	desktop = desktop_for_client(c);

	return desktop == -1 || desktop == wm.current_desktop;
}

int
desktop_offset(int desktop, int offset)
{
	return (desktop + offset + wm.config.desktops) % wm.config.desktops;
}

void
desktop_update_workarea(void)
{
	unsigned long area[MAX_DESKTOPS * 4] = {0};
	unsigned long screen[2] = {0};
	int i;

	for (i = 0; i < wm.config.desktops; i++) {
		area[i * 4 + 2] = (unsigned long)wm.width;
		area[i * 4 + 3] = (unsigned long)desktop_work_height();
	}

	screen[0] = (unsigned long)wm.width;
	screen[1] = (unsigned long)wm.height;
	wm_property_set_cardinals(
		wm.root, wm.atoms[A_WORKAREA], area, wm.config.desktops * 4);
	wm_property_set_cardinals(
		wm.root, wm.atoms[A_DESKTOP_GEOMETRY], screen, 2);
}

void
desktop_publish(void)
{
	unsigned long count, current;
	unsigned long viewport[MAX_DESKTOPS * 2] = {0};
	int i, length;
	char names[MAX_DESKTOPS * 16];

	count = (unsigned long)wm.config.desktops;
	current = (unsigned long)wm.current_desktop;

	wm_property_set_cardinals(
		wm.root, wm.atoms[A_DESKTOP_COUNT], &count, 1);
	wm_property_set_cardinals(
		wm.root, wm.atoms[A_CURRENT_DESKTOP], &current, 1);
	wm_property_set_cardinals(
		wm.root, wm.atoms[A_LAST_DESKTOP], &current, 1);
	wm_property_set_cardinals(wm.root, wm.atoms[A_DESKTOP_VIEWPORT],
		viewport, wm.config.desktops * 2);

	length = 0;

	for (i = 0; i < wm.config.desktops; i++)
		length +=
			snprintf(names + length, sizeof(names) - (size_t)length,
				"Desktop %d", i + 1) +
			1;

	XChangeProperty(wm.display, wm.root, wm.atoms[A_DESKTOP_NAMES],
		wm.atoms[A_UTF8], 8, PropModeReplace, (unsigned char *)names,
		length);
	desktop_update_workarea();
}

void
desktop_init(void)
{
	unsigned long n;
	unsigned char *data = NULL;

	data = wm_property_get(
		wm.root, wm.atoms[A_LAST_DESKTOP], XA_CARDINAL, 32, &n);

	if (data != NULL && n) {
		unsigned long desktop;

		desktop = ((unsigned long *)data)[0];

		wm.current_desktop = desktop < (unsigned long)wm.config.desktops
			? (int)desktop
			: wm.config.desktops - 1;
	}

	if (data != NULL)
		XFree(data);
	desktop_publish();
}

void
desktop_switch(int desktop, Time time)
{
	struct client *c = NULL;

	if (desktop < 0 || desktop >= wm.config.desktops ||
		desktop == wm.current_desktop)
		return;

	window_frame_cancel_drag(NULL);
	window_move_finish(1);
	owm_menu_close();
	desktop_icons_cancel_drag();
	window_frame_reset_click();
	wm.desktop_focus[wm.current_desktop] =
		(wm.focused != NULL) ? wm.focused->window : None;
	window_client_focus(NULL, time);
	wm.current_desktop = desktop;
	desktop_publish();
	window_client_sync_visibility();

	c = window_client_find(wm.desktop_focus[desktop]);

	if (c != NULL && window_client_visible(c))
		window_client_focus(c, time);
	else
		window_client_focus_topmost();
	taskbar_draw();
}

void
desktop_send(struct client *c, int desktop, Time time)
{
	const struct client *owner = NULL;
	struct client *p = NULL;

	if (c == NULL || desktop < -1 || desktop >= wm.config.desktops)
		return;

	owner = desktop_owner(c);

	window_frame_cancel_drag(NULL);
	desktop_icons_cancel_drag();
	window_frame_reset_click();

	for (p = wm.clients; p != NULL; p = p->next)
		if (desktop_owner(p) == owner)
			p->desktop = desktop;

	window_client_sync_visibility();

	if (wm.focused != NULL && !window_client_visible(wm.focused))
		window_client_focus_topmost();
	else if (wm.focused != NULL)
		window_client_focus(wm.focused, time);
	taskbar_draw();
}
