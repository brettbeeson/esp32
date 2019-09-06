#pragma once

#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_softap.h>
#include <freertos/event_groups.h>

#ifdef __cplusplus
extern "C" {
#endif

/* FreeRTOS event group to signal when we are connected*/
extern EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about one event 
 * - are we connected to the AP with an IP? */
const int WIFI_CONNECTED_BIT = BIT0;

/* Event handler for catching system events */
esp_err_t event_handler(void *ctx, system_event_t *event);

/* Event handler for catching provisioning manager events */
void prov_event_handler(void *user_data,wifi_prov_cb_event_t event, void *event_data);

#ifdef __cplusplus
}
#endif