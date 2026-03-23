#ifndef CLI_MENU_H
#define CLI_MENU_H

typedef enum {
    CLI_MENU_MAIN = 0,
    CLI_MENU_A,
    CLI_MENU_B,
    CLI_MENU_C,
    CLI_MENU_D,
} cli_menu_t;

void cli_menu_set(cli_menu_t menu);
cli_menu_t cli_menu_get(void);
void cli_menu_render(void);
void cli_menu_show_main(void);

#endif
