#ifndef OWM_TRAY_WIDGETS_H
#define OWM_TRAY_WIDGETS_H

enum {
	TRAY_ICON_NONE = -1,
	TRAY_ICON_BATTERY,
	TRAY_ICON_POWER,
	TRAY_ICON_NETWORK,
	TRAY_ICON_COUNT
};

struct tray_widget;

struct tray_widget {
	char		text[64];
	char		tooltip[128];
	int		icon;
	int		width;
	double		next_update;
	int		interval;

	void		(*update)(struct tray_widget *, double);
	const int	*enabled;
};

int	tray_widget_icon_available(int);
void	tray_battery_update(struct tray_widget *, double);
void	tray_clock_update(struct tray_widget *, double);

void	tray_widgets_init(void);
void	tray_widgets_free(void);
void	tray_widgets_update(double);
double	tray_widgets_deadline(void);
int	tray_widgets_left(void);
void	tray_widgets_draw(void);
/* Borrowed text remains valid until the next widget update or teardown. */
const char	*tray_widgets_tooltip(int, int);

#endif
