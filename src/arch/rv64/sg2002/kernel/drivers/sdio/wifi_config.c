#include "wifi_config.h"
#include "../../../../arch/rv64/core/sageos_build/kernel/include/vfs.h"
#include <string.h>

// Minimal parser, assuming ssid=... and password=... on separate lines
int load_wifi_config(wifi_config_t* config) {
    char buffer[256];
    int bytes_read = vfs_read("/sd/wifi_config.conf", 0, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) return -1;
    buffer[bytes_read] = '\0';

    char *line = strtok(buffer, "\n");
    while (line != NULL) {
        if (strncmp(line, "ssid=", 5) == 0) {
            strncpy(config->ssid, line + 5, sizeof(config->ssid) - 1);
        } else if (strncmp(line, "password=", 9) == 0) {
            strncpy(config->password, line + 9, sizeof(config->password) - 1);
        }
        line = strtok(NULL, "\n");
    }
    
    return 0;
}
