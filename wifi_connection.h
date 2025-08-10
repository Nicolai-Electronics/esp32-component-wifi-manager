#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_wifi.h"

#define WIFI_INFINITE_RETRIES 255

void                 wifi_connection_init_stack(void);
esp_err_t            wifi_connection_connect(uint16_t index, uint8_t max_retries);
void                 wifi_connection_disconnect(void);
bool                 wifi_connection_await(uint64_t max_delay_millis);
bool                 wifi_connection_is_connected(void);
esp_netif_ip_info_t* wifi_get_ip_info(void);
esp_netif_t*         wifi_get_netif(void);
esp_err_t            wifi_connect_try_all(void);