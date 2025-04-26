#include "wifi_settings.h"
#include <assert.h>
#include <inttypes.h>
#include <string.h>
#include "esp_eap_client.h"
#include "esp_err.h"
#include "esp_wifi_types_generic.h"
#include "nvs.h"

#define member_size(type, member) (sizeof(((type*)0)->member))

static const char* NVS_NAMESPACE = "wifi";

static void wifi_settings_combine_key(uint8_t index, const char* parameter, char* out_nvs_key) {
    assert(snprintf(out_nvs_key, 16, "s%02x.%s", index, parameter) <= 16);
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

static esp_err_t _wifi_settings_get(nvs_handle_t nvs_handle, uint8_t index, wifi_settings_t* out_settings) {
    char buffer[128 + sizeof('\0')] = {0};

    // Check parameters
    if (out_settings == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(out_settings, 0, sizeof(wifi_settings_t));

    // Read SSID (32 bytes)
    esp_err_t res =
        wifi_settings_get_parameter_str(nvs_handle, index, "ssid", buffer, member_size(wifi_settings_t, ssid) + 1);
    if (res != ESP_OK) {
        return res;
    }
    memcpy(out_settings->ssid, buffer, member_size(wifi_settings_t, ssid));
    memset(buffer, 0, sizeof(buffer));

    // Read password (64 bytes)
    res = wifi_settings_get_parameter_str(nvs_handle, index, "password", buffer,
                                          member_size(wifi_settings_t, password) + 1);
    if (res != ESP_OK) {
        return res;
    }
    memcpy(out_settings->password, buffer, member_size(wifi_settings_t, password));
    memset(buffer, 0, sizeof(buffer));

    // Read identity (128 bytes)
    res = wifi_settings_get_parameter_str(nvs_handle, index, "identity", buffer,
                                          member_size(wifi_settings_t, identity) + 1);
    if (res != ESP_OK) {
        return res;
    }
    memcpy(out_settings->identity, buffer, member_size(wifi_settings_t, identity));
    memset(buffer, 0, sizeof(buffer));

    // Read username (128 bytes)
    res = wifi_settings_get_parameter_str(nvs_handle, index, "username", buffer,
                                          member_size(wifi_settings_t, username) + 1);
    if (res != ESP_OK) {
        return res;
    }
    memcpy(out_settings->username, buffer, member_size(wifi_settings_t, username));
    memset(buffer, 0, sizeof(buffer));

    // Read authmode (enum, stored as u32)
    uint32_t authmode = 0;
    res               = wifi_settings_get_parameter_u32(nvs_handle, index, "authmode", &authmode);
    if (res != ESP_OK) {
        return res;
    }
    out_settings->authmode = (wifi_auth_mode_t)authmode;

    // Read phase2 (enum, stored as u32)
    uint32_t phase2 = 0;
    res             = wifi_settings_get_parameter_u32(nvs_handle, index, "phase2", &phase2);
    if (res != ESP_OK) {
        return res;
    }
    out_settings->phase2 = (esp_eap_ttls_phase2_types)phase2;

    return res;
}

static esp_err_t _wifi_settings_set(nvs_handle_t nvs_handle, uint8_t index, wifi_settings_t* settings) {
    char buffer[128 + 1] = {0};

    // Check parameters
    if (settings == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Write SSID (32 bytes)
    memcpy(buffer, settings->ssid, member_size(wifi_settings_t, ssid));
    buffer[member_size(wifi_settings_t, ssid)] = '\0';
    esp_err_t res =
        wifi_settings_set_parameter_str(nvs_handle, index, "ssid", buffer, member_size(wifi_settings_t, ssid) + 1);
    if (res != ESP_OK) {
        return res;
    }

    // Write password (64 bytes)
    memcpy(buffer, settings->password, member_size(wifi_settings_t, password));
    buffer[member_size(wifi_settings_t, password)] = '\0';
    res = wifi_settings_set_parameter_str(nvs_handle, index, "password", buffer,
                                          member_size(wifi_settings_t, password) + 1);
    if (res != ESP_OK) {
        return res;
    }

    // Write identity (128 bytes)
    memcpy(buffer, settings->identity, member_size(wifi_settings_t, identity));
    buffer[member_size(wifi_settings_t, identity)] = '\0';
    res = wifi_settings_set_parameter_str(nvs_handle, index, "identity", buffer,
                                          member_size(wifi_settings_t, identity) + 1);
    if (res != ESP_OK) {
        return res;
    }

    // Write username (128 bytes)
    memcpy(buffer, settings->username, member_size(wifi_settings_t, username));
    buffer[member_size(wifi_settings_t, username)] = '\0';
    res = wifi_settings_set_parameter_str(nvs_handle, index, "username", buffer,
                                          member_size(wifi_settings_t, username) + 1);
    if (res != ESP_OK) {
        return res;
    }

    // Write authmode (enum, stored as u32)
    uint32_t authmode = (uint32_t)settings->authmode;
    res               = wifi_settings_set_parameter_u32(nvs_handle, index, "authmode", authmode);
    if (res != ESP_OK) {
        return res;
    }

    // Write phase2 (enum, stored as u32)
    uint32_t phase2 = (uint32_t)settings->phase2;
    res             = wifi_settings_set_parameter_u32(nvs_handle, index, "phase2", phase2);
    if (res != ESP_OK) {
        return res;
    }

    return res;
}

static esp_err_t _wifi_settings_erase(nvs_handle_t nvs_handle, uint8_t index) {
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
    return ESP_OK;
}

esp_err_t wifi_settings_get(uint8_t index, wifi_settings_t* out_settings) {
    if (out_settings == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t    res = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (res != ESP_OK) {
        return res;
    }

    res = _wifi_settings_get(nvs_handle, index, out_settings);
    nvs_close(nvs_handle);
    return res;
}

esp_err_t wifi_settings_set(uint8_t index, wifi_settings_t* settings) {
    if (settings == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t    res = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (res != ESP_OK) {
        return res;
    }

    res = _wifi_settings_set(nvs_handle, index, settings);
    if (res != ESP_OK) {
        nvs_close(nvs_handle);
        return res;
    }

    res = nvs_commit(nvs_handle);
    if (res != ESP_OK) {
        nvs_close(nvs_handle);
        return res;
    }

    nvs_close(nvs_handle);
    return res;
}

esp_err_t wifi_settings_erase(uint8_t index) {
    nvs_handle_t nvs_handle;
    esp_err_t    res = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (res != ESP_OK) {
        return res;
    }

    res = _wifi_settings_erase(nvs_handle, index);
    if (res != ESP_OK) {
        nvs_close(nvs_handle);
        return res;
    }

    uint8_t move_index = index;
    while (1) {
        wifi_settings_t temp = {0};
        res                  = _wifi_settings_get(nvs_handle, move_index + 1, &temp);
        if (res != ESP_OK) {
            break;
        }
        res = _wifi_settings_set(nvs_handle, move_index, &temp);
        move_index++;
    }

    if (move_index != index) {
        res = _wifi_settings_erase(nvs_handle, move_index);
        if (res != ESP_OK) {
            nvs_close(nvs_handle);
            return res;
        }
    }

    res = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return res;
}

int wifi_settings_find_empty_slot(void) {
    int slot = -1;
    for (uint32_t index = 0; index < WIFI_SETTINGS_MAX; index++) {
        wifi_settings_t settings;
        esp_err_t       res = wifi_settings_get(index, &settings);
        if (res != ESP_OK) {
            slot = index;
            break;
        }
    }
    return slot;
}