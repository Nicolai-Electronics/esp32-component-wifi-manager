#include "wifi_settings.h"
#include <inttypes.h>
#include <string.h>
#include "esp_eap_client.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi_types_generic.h"
#include "nvs.h"

#define member_size(type, member) (sizeof(((type*)0)->member))

static const char* TAG           = "WiFi settings";
static const char* NVS_NAMESPACE = "wifi";

static void wifi_settings_combine_key(uint8_t index, const char* parameter, char* out_nvs_key) {
    // Maximum parameter string length is 8
    // NVS key variable must be 16 bytes long
    snprintf(out_nvs_key, 16, "sta.%03u.%s", index, parameter);
}

static esp_err_t wifi_settings_get_parameter_str(nvs_handle_t nvs_handle, uint8_t index, const char* parameter,
                                                 char* out_string, size_t max_length) {
    char nvs_key[16];
    wifi_settings_combine_key(index, parameter, nvs_key);
    size_t    size = 0;
    esp_err_t res  = nvs_get_str(nvs_handle, nvs_key, NULL, &size);
    if (res != ESP_OK) {
        return res;
    }
    if (size > max_length) {
        return ESP_ERR_NO_MEM;
    }
    return nvs_get_str(nvs_handle, nvs_key, out_string, &size);
}

static esp_err_t wifi_settings_set_parameter_str(nvs_handle_t nvs_handle, uint8_t index, const char* parameter,
                                                 char* string, size_t max_length) {
    char nvs_key[16];
    wifi_settings_combine_key(index, parameter, nvs_key);
    return nvs_set_str(nvs_handle, nvs_key, string);
}

static esp_err_t wifi_settings_get_parameter_u32(nvs_handle_t nvs_handle, uint8_t index, const char* parameter,
                                                 uint32_t* out_value) {
    char nvs_key[16];
    wifi_settings_combine_key(index, parameter, nvs_key);
    return nvs_get_u32(nvs_handle, nvs_key, out_value);
}

static esp_err_t wifi_settings_set_parameter_u32(nvs_handle_t nvs_handle, uint8_t index, const char* parameter,
                                                 uint32_t value) {
    char nvs_key[16];
    wifi_settings_combine_key(index, parameter, nvs_key);
    return nvs_set_u32(nvs_handle, nvs_key, value);
}

esp_err_t wifi_settings_get(uint8_t index, wifi_settings_t* out_settings) {
    char buffer[128 + 1] = {0};

    // Check parameters
    if (out_settings == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Open the NVS namespace
    nvs_handle_t nvs_handle;
    esp_err_t    res = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace");
        return res;
    }

    // Read SSID (32 bytes)
    res = wifi_settings_get_parameter_str(nvs_handle, index, "ssid", buffer, member_size(wifi_settings_t, ssid) + 1);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read ssid");
        goto exit;
    }
    memcpy(out_settings->ssid, buffer, member_size(wifi_settings_t, ssid));
    memset(buffer, 0, sizeof(buffer));

    // Read password (64 bytes)
    res = wifi_settings_get_parameter_str(nvs_handle, index, "password", buffer,
                                          member_size(wifi_settings_t, password) + 1);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read password");
        goto exit;
    }
    memcpy(out_settings->password, buffer, member_size(wifi_settings_t, password));
    memset(buffer, 0, sizeof(buffer));

    // Read identity (128 bytes)
    res = wifi_settings_get_parameter_str(nvs_handle, index, "identity", buffer,
                                          member_size(wifi_settings_t, identity) + 1);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read identity");
        goto exit;
    }
    memcpy(out_settings->identity, buffer, member_size(wifi_settings_t, identity));
    memset(buffer, 0, sizeof(buffer));

    // Read username (128 bytes)
    res = wifi_settings_get_parameter_str(nvs_handle, index, "username", buffer,
                                          member_size(wifi_settings_t, username) + 1);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read username");
        goto exit;
    }
    memcpy(out_settings->username, buffer, member_size(wifi_settings_t, username));
    memset(buffer, 0, sizeof(buffer));

    // Read authmode (enum, stored as u32)
    uint32_t authmode = 0;
    res               = wifi_settings_get_parameter_u32(nvs_handle, index, "authmode", &authmode);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read authmode");
        goto exit;
    }
    out_settings->authmode = (wifi_auth_mode_t)authmode;

    // Read phase2 (enum, stored as u32)
    uint32_t phase2 = 0;
    res             = wifi_settings_get_parameter_u32(nvs_handle, index, "phase2", &phase2);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read phase2");
        goto exit;
    }
    out_settings->phase2 = (esp_eap_ttls_phase2_types)phase2;

exit:
    nvs_close(nvs_handle);
    return res;
}

esp_err_t wifi_settings_set(uint8_t index, wifi_settings_t* settings) {
    char buffer[128 + 1] = {0};

    // Check parameters
    if (settings == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Open the NVS namespace
    nvs_handle_t nvs_handle;
    esp_err_t    res = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace");
        return res;
    }

    // Write SSID (32 bytes)
    memcpy(buffer, settings->ssid, member_size(wifi_settings_t, ssid));
    buffer[member_size(wifi_settings_t, ssid)] = '\0';
    res = wifi_settings_set_parameter_str(nvs_handle, index, "ssid", buffer, member_size(wifi_settings_t, ssid) + 1);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to store ssid");
        goto exit;
    }

    // Write password (64 bytes)
    memcpy(buffer, settings->password, member_size(wifi_settings_t, password));
    buffer[member_size(wifi_settings_t, password)] = '\0';
    res = wifi_settings_set_parameter_str(nvs_handle, index, "password", buffer,
                                          member_size(wifi_settings_t, password) + 1);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to store password");
        goto exit;
    }

    // Write identity (128 bytes)
    memcpy(buffer, settings->identity, member_size(wifi_settings_t, identity));
    buffer[member_size(wifi_settings_t, identity)] = '\0';
    res = wifi_settings_set_parameter_str(nvs_handle, index, "identity", buffer,
                                          member_size(wifi_settings_t, identity) + 1);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to store identity");
        goto exit;
    }

    // Write username (128 bytes)
    memcpy(buffer, settings->username, member_size(wifi_settings_t, username));
    buffer[member_size(wifi_settings_t, username)] = '\0';
    res = wifi_settings_set_parameter_str(nvs_handle, index, "username", buffer,
                                          member_size(wifi_settings_t, username) + 1);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to store username");
        goto exit;
    }

    // Write authmode (enum, stored as u32)
    uint32_t authmode = (uint32_t)settings->authmode;
    res               = wifi_settings_set_parameter_u32(nvs_handle, index, "authmode", authmode);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to store authmode");
        goto exit;
    }

    // Write phase2 (enum, stored as u32)
    uint32_t phase2 = (uint32_t)settings->phase2;
    res             = wifi_settings_set_parameter_u32(nvs_handle, index, "phase2", phase2);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to store phase2");
        goto exit;
    }

    res = nvs_commit(nvs_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit to NVS");
        goto exit;
    }

exit:
    nvs_close(nvs_handle);
    return res;
}

esp_err_t wifi_settings_erase(uint8_t index) {
    // Open the NVS namespace
    nvs_handle_t nvs_handle;
    esp_err_t    res = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (res != ESP_OK) return res;

    char nvs_key[16];
    wifi_settings_combine_key(index, "ssid", nvs_key);
    nvs_erase_key(nvs_handle, nvs_key);
    wifi_settings_combine_key(index, "password", nvs_key);
    nvs_erase_key(nvs_handle, nvs_key);
    wifi_settings_combine_key(index, "identity", nvs_key);
    nvs_erase_key(nvs_handle, nvs_key);
    wifi_settings_combine_key(index, "username", nvs_key);
    nvs_erase_key(nvs_handle, nvs_key);
    wifi_settings_combine_key(index, "authmode", nvs_key);
    nvs_erase_key(nvs_handle, nvs_key);
    wifi_settings_combine_key(index, "phase2", nvs_key);
    nvs_erase_key(nvs_handle, nvs_key);

    res = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return res;
}
