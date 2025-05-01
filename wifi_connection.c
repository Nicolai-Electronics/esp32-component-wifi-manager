#include "wifi_connection.h"
#include <stdbool.h>
#include <string.h>
#include "esp_check.h"
#include "esp_eap_client.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_wifi_types_generic.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "wifi_settings.h"

static const char* TAG = "WiFi connection";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define WIFI_STARTED_BIT   BIT2

static EventGroupHandle_t wifiEventGroup = NULL;

static uint8_t m_retry_count = 0;
static uint8_t m_max_retries = 3;
static bool    m_is_scanning = false;

static esp_netif_ip_info_t ip_info = {0};

// Handles WiFi events required to stay connected.
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        xEventGroupSetBits(wifiEventGroup, WIFI_STARTED_BIT);
        if (!m_is_scanning) {
            // Connect only if we're not scanning the WiFi.
            esp_wifi_connect();
        }
        ESP_LOGI(TAG, "WiFi station start");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_STOP) {
        xEventGroupClearBits(wifiEventGroup, WIFI_STARTED_BIT);
        ESP_LOGI(TAG, "WiFi station stop");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (m_max_retries == WIFI_INFINITE_RETRIES || m_retry_count < m_max_retries) {
            esp_wifi_connect();
            m_retry_count++;
            ESP_LOGI(TAG, "Retrying connection");
        } else {
            ESP_LOGI(TAG, "Connection failed");
            xEventGroupSetBits(wifiEventGroup, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        memcpy(&ip_info, &event->ip_info, sizeof(ip_info));
        ESP_LOGI(TAG, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
        ESP_LOGI(TAG, "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
        m_retry_count = 0;
        xEventGroupSetBits(wifiEventGroup, WIFI_CONNECTED_BIT);
    }
}

static void wifi_connection_init_internal(void) {
    if (wifiEventGroup != NULL) {
        return;  // Already initialized
    }
    wifiEventGroup = xEventGroupCreate();
    assert(wifiEventGroup);
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL));
}

void wifi_connection_init_stack(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.nvs_enable         = false;  // Do not store WiFi credentials in NVS of ESP32-C6 radio
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_stop());

    wifi_connection_init_internal();
}

// Connect to WiFi using the WiFi settings stored a specific settings slot
esp_err_t wifi_connection_connect(uint16_t index, uint8_t max_retries) {
    wifi_settings_t settings;
    esp_err_t       res = wifi_settings_get(index, &settings);
    if (res != ESP_OK) {
        return res;
    }

    wifi_connection_init_internal();
    m_max_retries = 0;
    m_max_retries = max_retries;
    esp_wifi_disconnect();
    esp_wifi_stop();
    xEventGroupClearBits(wifiEventGroup, 0xFF);

    wifi_config_t wifi_config = {0};

    // Both the SSID and the password fields do not need to be NULL terminated
    memcpy((char*)wifi_config.sta.ssid, settings.ssid, strnlen((char*)settings.ssid, 32));
    memcpy((char*)wifi_config.sta.password, settings.password, strnlen((char*)settings.password, 64));
    wifi_config.sta.threshold.authmode = settings.authmode;

    bool use_eap;  // Flag indicating enterprise type security is used

    switch (settings.authmode) {
        case WIFI_AUTH_ENTERPRISE:
        case WIFI_AUTH_WPA3_ENTERPRISE:
        case WIFI_AUTH_WPA2_WPA3_ENTERPRISE:
            use_eap = true;
            break;
        default:
            use_eap = false;
            break;
    }

#ifndef CONFIG_IDF_TARGET_ESP32P4
    if (use_eap) {
        ESP_RETURN_ON_ERROR(
            esp_eap_client_set_identity((unsigned char*)settings.identity, strnlen((char*)settings.identity, 128)), TAG,
            "Failed to set identity");
        ESP_RETURN_ON_ERROR(
            esp_eap_client_set_username((unsigned char*)settings.username, strnlen((char*)settings.username, 128)), TAG,
            "Failed to set username");
        ESP_RETURN_ON_ERROR(
            esp_eap_client_set_password((unsigned char*)settings.password, strnlen((char*)settings.password, 64)), TAG,
            "Failed to set password");
        ESP_RETURN_ON_ERROR(esp_eap_client_set_ttls_phase2_method(settings.phase2), TAG, "Failed to set phase2 method");
        ESP_RETURN_ON_ERROR(esp_wifi_sta_enterprise_enable(), TAG, "Failed to enable enterprise mode");
    } else {
        ESP_RETURN_ON_ERROR(esp_wifi_sta_enterprise_disable(), TAG, "Failed to disable enterprise mode");
    }
#else
    if (use_eap) {
        ESP_LOGE(TAG, "Connecting to enterprise networks is not yet supported");
        return ESP_ERR_NOT_SUPPORTED;
    }
#endif

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "Failed to set WiFi mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG, "Failed to set WiFi configuration");

#ifndef CONFIG_IDF_TARGET_ESP32P4
    esp_wifi_config_11b_rate(WIFI_IF_STA,
                             true);  // Disable 802.11B type WiFi support
#endif

    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Failed to start WiFi");
    ESP_LOGI(TAG, "Connecting to WiFi...");
    return ESP_OK;
}

// Disconnect from WiFi
void wifi_connection_disconnect(void) {
    m_max_retries = 0;
    esp_wifi_stop();
}

// Awaits WiFi to be connected for at most `max_delay_millis` milliseconds.
bool wifi_connection_await(uint64_t max_delay_millis) {
    if (!max_delay_millis) {
        max_delay_millis = portMAX_DELAY;
    } else {
        max_delay_millis = pdMS_TO_TICKS(max_delay_millis);
    }

    for (;;) {
        EventBits_t bits =
            xEventGroupWaitBits(wifiEventGroup, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, 0, 0, max_delay_millis);
        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "Connected to WiFi");
            return true;
        } else if (bits & WIFI_FAIL_BIT) {
            ESP_LOGE(TAG, "Failed to connect");
        } else if (WIFI_STARTED_BIT) {
            ESP_LOGI(TAG, "WiFi started");
            continue;
        } else {
            ESP_LOGE(TAG, "Unknown event received while waiting on connection (0x%x)", bits);
        }
        break;
    }
    return false;
}

bool wifi_connection_is_connected(void) {
    uint32_t bits = xEventGroupGetBits(wifiEventGroup) & WIFI_CONNECTED_BIT;
    return (bits & WIFI_CONNECTED_BIT);
}

esp_netif_ip_info_t* wifi_get_ip_info(void) {
    return &ip_info;
}

esp_err_t wifi_connect_try_all(void) {
    for (uint16_t index = 0; index < WIFI_SETTINGS_MAX; index++) {
        if (wifi_connection_connect(index, 3) == ESP_OK) {
            ESP_LOGI(TAG, "Connecting to network in slot %" PRIu32, index);
            if (wifi_connection_await(500)) {
                return ESP_OK;
            }
        } else {
            ESP_LOGI(TAG, "No network stored in slot %" PRIu32, index);
        }
    }
    ESP_LOGE(TAG, "Tried all stored networks, unable to connect");
    return ESP_FAIL;
}
