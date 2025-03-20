#pragma once
#include "esp_eap_client.h"
#include "esp_err.h"
#include "esp_wifi_types_generic.h"

#define WIFI_SETTINGS_MAX 0xFF

typedef struct {
    // Basic network information
    uint8_t                   ssid[32];
    wifi_auth_mode_t          authmode;
    // Password for secure WiFi networks
    uint8_t                   password[64];
    // Enterprise WiFi configuration
    uint8_t                   identity[128];
    uint8_t                   username[128];
    esp_eap_ttls_phase2_types phase2;
} wifi_settings_t;

esp_err_t wifi_settings_get(uint8_t index, wifi_settings_t* out_settings);
esp_err_t wifi_settings_set(uint8_t index, wifi_settings_t* settings);
esp_err_t wifi_settings_erase(uint8_t index);
