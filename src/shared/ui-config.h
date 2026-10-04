#ifndef OWM_UI_CONFIG_H
#define OWM_UI_CONFIG_H

#define DEFAULT_FONT "Liberation Mono:size=10"
#define FONT_NAME_SIZE 256

/* Invalid values leave the destination unchanged. */
int	ui_config_parse_font(char[FONT_NAME_SIZE], const char *);
/* Load the shared UI option, using the default when absent. */
void	ui_config_font(char[FONT_NAME_SIZE]);

#endif
