#include "cli_core.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include <esp_log.h>

#include "cli_commands.h"

static const char *TAG = "cli_core";

static void trim_leading_spaces(char **p) {
    while (**p == ' ' || **p == '\t') {
        (*p)++;
    }
}

void cli_execute(const char *line) {
    char buf[128];
    char *cmd;
    char *args;
    size_t count = 0;
    const cli_command_t *cmds = cli_get_commands(&count);

    if (!line || line[0] == '\0') {
        ESP_LOGI(TAG, "empty line");
        return;
    }

    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    cmd = buf;
    trim_leading_spaces(&cmd);
    if (*cmd == '\0') {
        ESP_LOGI(TAG, "empty command");
        return;
    }

    args = cmd;
    while (*args && *args != ' ' && *args != '\t') {
        args++;
    }
    if (*args) {
        *args = '\0';
        args++;
        trim_leading_spaces(&args);
    }

    if ((!args || *args == '\0') && *cmd && strspn(cmd, "0123456789") == strlen(cmd)) {
        long idx = strtol(cmd, NULL, 10);
        if (idx >= 1 && (size_t)idx <= count) {
            cmds[idx - 1].fn("");
            return;
        }
        ESP_LOGI(TAG, "invalid menu number: %s", cmd);
        return;
    }

    for (size_t i = 0; i < count; i++) {
        if (strcasecmp(cmd, cmds[i].name) == 0) {
            cmds[i].fn(args ? args : "");
            return;
        }
    }

    ESP_LOGI(TAG, "unknown command: %s (type HELP)", cmd);
}
