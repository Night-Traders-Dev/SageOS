#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

typedef struct {
    char ssid[64];
    char password[64];
} wifi_config_t;

int load_wifi_config(wifi_config_t* config);

#endif
