#include "ui-colors.h"

const char *
ui_color(int color)
{
	static const char *palette[COLOR_COUNT] = {"#3e515b", "#aca99f",
		"#f6f3e8", "#d6d2c7", "#65655e", "#252722", "#526b78"};

	return palette[color];
}
