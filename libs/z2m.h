//
// Created by emeka.ariwodo on 3/16/2026.
//

#ifndef Z2M_H
#define Z2M_H
#include "esp_zigbee_core.h"
#include "esp_check.h"

#define EP_TEMPSENSOR_1 1
#define EP_TEMPSENSOR_2 2
#define EP_TEMPSENSOR_3 3
#define EP_TEMPSENSOR_4 4
#define CUSTOM_CLUSTER_ID 0xff00
#define CUSTOM_COMMAND_RESP 0x0001
#define HA_ESP_SENSOR_ENDPOINT 1


void register_device();

void z2m_main_loop();

static void esp_zb_task(void *pvParameters);

#define ESP_ZB_ZED_CONFIG()                                     \
{                                                               \
    .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED,                       \
    .install_code_policy = false,           \
    .nwk_cfg.zed_cfg = {                                        \
        .ed_timeout = ESP_ZB_ED_AGING_TIMEOUT_64MIN,                         \
        .keep_alive = 3000,                            \
    },                                                          \
}

#endif //Z2M_H