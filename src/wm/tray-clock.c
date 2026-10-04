#include <time.h>

#include "tray-widgets.h"

void
tray_clock_update(struct tray_widget *widget, double unused)
{
	struct tm local;
	time_t now;

	(void)unused;

	now = time(NULL);

	widget->text[0] = widget->tooltip[0] = '\0';

	if (localtime_r(&now, &local) == NULL)
		return;

	strftime(widget->text, sizeof(widget->text), "%H:%M", &local);
	strftime(widget->tooltip, sizeof(widget->tooltip), "%A, %d %B %Y",
		&local);
}
