#include "wifi_prov.h"
#include <esp_wifi.h>
#include <string.h>

static const char *TAG = "wifi_prov";
#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

EventGroupHandle_t wifi_event_group=0;

/* Event handler for catching system events */
 esp_err_t event_handler(void *ctx, system_event_t *event)
{
    /* Pass event information to provisioning manager so that it can
     * maintain its internal state depending upon the system event */
    wifi_prov_mgr_event_handler(ctx, event);

    /* Global event handling */
    switch (event->event_id) {
        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "Connected with IP Address:%s",
                     ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
            xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT );
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "Disconnected. Connecting to the AP again...");
            esp_wifi_connect();
            break;
        default:
            break;
    }
    return ESP_OK;
}

/* Event handler for catching provisioning manager events */
 void prov_event_handler(void *user_data,
                               wifi_prov_cb_event_t event, void *event_data)
{
    switch (event) {
        case WIFI_PROV_START:
            ESP_LOGI(TAG, "Provisioning started");
            break;
        case WIFI_PROV_CRED_RECV: {
            wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
            /* If SSID length is exactly 32 bytes, null termination
             * will not be present, so explicitly obtain the length */
            size_t ssid_len = strnlen((const char *)wifi_sta_cfg->ssid, sizeof(wifi_sta_cfg->ssid));
            ESP_LOGI(TAG, "Received Wi-Fi credentials"
                     "\n\tSSID     : %.*s\n\tPassword : %s",
                     ssid_len, (const char *) wifi_sta_cfg->ssid,
                     (const char *) wifi_sta_cfg->password);
            break;
        }
        case WIFI_PROV_CRED_FAIL: {
            wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
            ESP_LOGE(TAG, "Provisioning failed!\n\tReason : %s"
                     "\n\tPlease reset to factory and retry provisioning",
                     (*reason == WIFI_PROV_STA_AUTH_ERROR) ?
                     "Wi-Fi AP password incorrect" : "Wi-Fi AP not found");
            break;
        }
        case WIFI_PROV_CRED_SUCCESS:
            ESP_LOGI(TAG, "Provisioning successful");
            break;
        case WIFI_PROV_END:
            /* De-initialize manager once provisioning is finished */
            wifi_prov_mgr_deinit();
            break;
        default:
            break;
    }
}

