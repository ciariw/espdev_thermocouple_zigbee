//
// Created by emeka.ariwodo on 3/16/2026.
//

#include "z2m.h"
#include "zcl/esp_zigbee_zcl_common.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "esp_log.h"
#include "esp_check.h"

char isactive[40];

#if TEMPERATURE_MODULES == 2
int16_t temperaturebuffer[8] = {0,0,0,0,0,0,0,0};
#else
int16_t temperaturebuffer[4] = {0,0,0,0};
#endif

typedef struct zbstring_s {
    uint8_t len;
    char data[];
} ESP_ZB_PACKED_STRUCT
zbstring_t;

static void update_tc_attr(uint16_t clusterID)
{
    esp_zb_lock_acquire(portMAX_DELAY);

    for (int index = 0; index < 4*TEMPERATURE_MODULES; index++)
    {
        esp_zb_zcl_status_t st = esp_zb_zcl_set_attribute_val(
            10,                              // endpoint
            clusterID,               // 0xFF00
            ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
            index,                         // 0x0000..0x0003
            &temperaturebuffer[index],
            false
        );
        if (st != 0)
        {
            ESP_LOGI("FAILED TO STORE DATA","Temperature channel %d", index );
        }

    }

    esp_zb_lock_release();
}

static void esp_app_temp_sensor_handler()
{
    esp_zb_lock_acquire(pdMS_TO_TICKS(10000));

    esp_zb_lock_release();
}

static void configure_local_reporting_for_attr(uint16_t attr_id)
{
    static int16_t change = 1;

    esp_zb_zcl_config_report_record_t rec = {
        .direction = ESP_ZB_ZCL_REPORT_DIRECTION_SEND,
        .attributeID = attr_id,
        .attrType = ESP_ZB_ZCL_ATTR_TYPE_S16,
        .min_interval = 1,
        .max_interval = 1,
        .reportable_change = &change,
    };

    esp_zb_zcl_config_report_cmd_t cmd = {0};
    cmd.zcl_basic_cmd.src_endpoint = 10;
    cmd.zcl_basic_cmd.dst_endpoint = 10;
    cmd.zcl_basic_cmd.dst_addr_u.addr_short = esp_zb_get_short_address();
    cmd.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    cmd.clusterID = CUSTOM_CLUSTER_ID;
    cmd.record_number = 1;
    cmd.record_field = &rec;

    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_config_report_cmd_req(&cmd);
    esp_zb_lock_release();
}

esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message)
{
    esp_err_t ret = ESP_OK;
    bool light_state = 0;

    ESP_RETURN_ON_FALSE(message, ESP_FAIL, "TAG", "Empty message");
    ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, "TAG", "Received message: error status(%d)",
                        message->info.status);
    ESP_LOGI("TAG", "Received message: endpoint(%d), cluster(0x%x), attribute(0x%x), data size(%d)", message->info.dst_endpoint, message->info.cluster,
             message->attribute.id, message->attribute.data.size);
    return ret;
}

static void add_reporting_slot(uint8_t ep, uint16_t cluster_id, uint16_t attr_id)
{
    esp_zb_zcl_reporting_info_t ri = {0};

    ri.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV;
    ri.ep = ep;
    ri.cluster_id = cluster_id;
    ri.cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE;
    ri.attr_id = attr_id;
    ri.manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC;
    ri.dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID;

    ri.u.send_info.min_interval = 10;
    ri.u.send_info.max_interval = 3600;
    ri.u.send_info.def_min_interval = 10;
    ri.u.send_info.def_max_interval = 3600;
    ri.u.send_info.delta.s16 = 1;
    ri.u.send_info.reported_value.s16 = 0;

    esp_err_t err = esp_zb_zcl_update_reporting_info(&ri);
    ESP_LOGI("ZB", "add report slot attr 0x%04X -> %s", attr_id, esp_err_to_name(err));
}

static void esp_app_zb_attribute_handler(uint16_t cluster_id, const esp_zb_zcl_attribute_t* attribute)
{
    char *TAG = "z2m";
    /* Basic cluster attributes */
    if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_BASIC) {
        if (attribute->id == ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID &&
            attribute->data.type == ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING &&
            attribute->data.value) {
            zbstring_t *zbstr = (zbstring_t *)attribute->data.value;
            char *string = (char*)malloc(zbstr->len + 1);
            memcpy(string, zbstr->data, zbstr->len);
            string[zbstr->len] = '\0';
            ESP_LOGI(TAG, "Peer Manufacturer is \"%s\"", string);
            free(string);
        }
        if (attribute->id == ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID &&
            attribute->data.type == ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING &&
            attribute->data.value) {
            zbstring_t *zbstr = (zbstring_t *)attribute->data.value;
            char *string = (char*)malloc(zbstr->len + 1);
            memcpy(string, zbstr->data, zbstr->len);
            string[zbstr->len] = '\0';
            ESP_LOGI(TAG, "Peer Model is \"%s\"", string);
            free(string);
        }
    }
    /* Temperature Measurement cluster attributes */
    if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT) {
        if (attribute->id == ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID &&
            attribute->data.type == ESP_ZB_ZCL_ATTR_TYPE_S16) {
            int16_t value = attribute->data.value ? *(int16_t *)attribute->data.value : 0;
            ESP_LOGI(TAG, "Measured Value is %.2f degrees Celsius", (value));
        }
        if (attribute->id == ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MIN_VALUE_ID &&
            attribute->data.type == ESP_ZB_ZCL_ATTR_TYPE_S16) {
            int16_t min_value = attribute->data.value ? *(int16_t *)attribute->data.value : 0;
            ESP_LOGI(TAG, "Min Measured Value is %.2f degrees Celsius", (min_value));
        }
        if (attribute->id == ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MAX_VALUE_ID &&
            attribute->data.type == ESP_ZB_ZCL_ATTR_TYPE_S16) {
            int16_t max_value = attribute->data.value ? *(int16_t *)attribute->data.value : 0;
            ESP_LOGI(TAG, "Max Measured Value is %.2f degrees Celsius", (max_value));
        }
        if (attribute->id == ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_TOLERANCE_ID &&
            attribute->data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
            uint16_t tolerance = attribute->data.value ? *(uint16_t *)attribute->data.value : 0;
            ESP_LOGI(TAG, "Tolerance is %.2f degrees Celsius", 1.0 * tolerance / 100);
        }
    }
}

static esp_err_t zb_attribute_reporting_handler(const esp_zb_zcl_report_attr_message_t *message)
{
    char *TAG = "z2m";
    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    ESP_RETURN_ON_FALSE(message->status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG, "Received message: error status(%d)",
                        message->status);
    ESP_LOGI(TAG, "Received report from address(0x%x) src endpoint(%d) to dst endpoint(%d) cluster(0x%x)",
             message->src_address.u.short_addr, message->src_endpoint,
             message->dst_endpoint, message->cluster);
    esp_app_zb_attribute_handler(message->cluster, &message->attribute);
    return ESP_OK;
}

static esp_err_t zb_configure_report_resp_handler(const esp_zb_zcl_cmd_config_report_resp_message_t *message)
{
    char *TAG = "z2m";
    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG, "Received message: error status(%d)",
                        message->info.status);
    esp_zb_zcl_config_report_resp_variable_t *variable = message->variables;
    while (variable) {
        ESP_LOGI(TAG, "Configure report response: status(%d), cluster(0x%x), direction(0x%x), attribute(0x%x)",
                 variable->status, message->info.cluster, variable->direction, variable->attribute_id);
        variable = variable->next;
    }
    return ESP_OK;
}

static esp_err_t zb_read_attr_resp_handler(const esp_zb_zcl_cmd_read_attr_resp_message_t *message)
{
    char *TAG = "z2m";
    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG, "Received message: error status(%d)",
                        message->info.status);
    ESP_LOGI(TAG, "Read attribute response: from address(0x%x) src endpoint(%d) to dst endpoint(%d) cluster(0x%x)",
             message->info.src_address.u.short_addr, message->info.src_endpoint,
             message->info.dst_endpoint, message->info.cluster);
    esp_zb_zcl_read_attr_resp_variable_t *variable = message->variables;
    while (variable) {
        ESP_LOGI(TAG, "Read attribute response: status(%d), cluster(0x%x), attribute(0x%x), type(0x%x), value(%d)",
                    variable->status, message->info.cluster,
                    variable->attribute.id, variable->attribute.data.type,
                    variable->attribute.data.value ? *(uint8_t *)variable->attribute.data.value : 0);
        if (variable->status == ESP_ZB_ZCL_STATUS_SUCCESS) {
            esp_app_zb_attribute_handler(message->info.cluster, &variable->attribute);
        }
        variable = variable->next;
    }
    return ESP_OK;
}

static void dump_reporting_info(uint8_t ep, uint16_t cluster_id, uint16_t attr_id)
{
    esp_zb_zcl_attr_location_info_t attr_info = {
        .endpoint_id = ep,
        .cluster_id = cluster_id,
        .cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        .attr_id = attr_id,
        .manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC,
    };
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_reporting_info_t *found = esp_zb_zcl_find_reporting_info(attr_info);
    esp_zb_lock_release();
    if (found) {
        ESP_LOGI("ZB",
                 "report slot exists ep=%u cluster=0x%04X attr=0x%04X min=%u max=%u",
                 ep,
                 cluster_id,
                 attr_id,
                 found->u.send_info.min_interval,
                 found->u.send_info.max_interval);
    } else {
        ESP_LOGW("ZB",
                 "no report slot for ep=%u cluster=0x%04X attr=0x%04X",
                 ep,
                 cluster_id,
                 attr_id);
    }
}

static esp_err_t esp_zb_zcl_cmd_default_resp_message_handler(const esp_zb_zcl_cmd_default_resp_message_t *message)
{
    const char* TAG = "Response message handle";
    ESP_LOGI(TAG, "Message attributes \nMessage response: %d\nDestination endpoint: %d\nCluster id: %d\nStatus code: (0x%x)",
        message->resp_to_cmd, message->info.dst_endpoint, message->info.cluster, message->status_code);
    if (message->info.cluster == CUSTOM_CLUSTER_ID)
    {
        update_tc_attr(CUSTOM_CLUSTER_ID);
        //temperature measurement message'
        //esp_app_temp_sensor_handler();

    }
    return ESP_OK;
}

esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    char *TAG = "z2m";
    esp_err_t ret = ESP_OK;
    switch (callback_id) {
    case ESP_ZB_CORE_REPORT_ATTR_CB_ID:
        ret = zb_attribute_reporting_handler((esp_zb_zcl_report_attr_message_t *)message);
        break;
    case ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID:
        ret = zb_read_attr_resp_handler((esp_zb_zcl_cmd_read_attr_resp_message_t *)message);
        break;
    case ESP_ZB_CORE_CMD_REPORT_CONFIG_RESP_CB_ID:
        ret = zb_configure_report_resp_handler((esp_zb_zcl_cmd_config_report_resp_message_t *)message);
        break;
    case ESP_ZB_CORE_CMD_DEFAULT_RESP_CB_ID:
        ret = esp_zb_zcl_cmd_default_resp_message_handler((esp_zb_zcl_cmd_default_resp_message_t *) message);
        break;
    default:
        ESP_LOGW(TAG, "Receive Zigbee action(0x%x) callback", callback_id);
        break;
    }
    return ret;
}

static esp_err_t deferred_driver_init(void){
    static bool is_inited = true;
    return is_inited ? ESP_OK : ESP_FAIL;
}

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    const char *TAG = "bdb_start_top_level_commissioning_cb";
    ESP_RETURN_ON_FALSE(esp_zb_bdb_start_top_level_commissioning(mode_mask) == ESP_OK, ,TAG, "Failed to start Zigbee bdb commissioning");
}

static void esp_zb_task(void *pvParameters)
{
    #if TEMPERATURE_MODULES == 2
    char modelid[] = {15, 'E', 'S', 'P', '3', '2', 'C', '6', '.', 'S', 'e', 'n', 's', 'o', 'r','2'};
    #else
    char modelid[] = {14, 'E', 'S', 'P', '3', '2', 'C', '6', '.', 'S', 'e', 'n', 's', 'o', 'r'};
    #endif
    char manufname[] = {9, 'E', 's', 'p', 'r', 'e', 's', 's', 'i', 'f'};
    uint8_t power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DC_SOURCE;
    esp_zb_zcl_attr_access_t aclist = ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING;

    esp_zb_identify_cluster_cfg_t identify_cluster_cfg = {
        .identify_time = 0,
    };

    esp_zb_endpoint_config_t endpoint_config = {
        .endpoint = ENDPOINT_TP,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_CUSTOM_ATTR_DEVICE_ID,
        .app_device_version = 0 };
    esp_zb_af_node_power_desc_t node_power_desc = {
        .current_power_mode = ESP_ZB_AF_NODE_POWER_MODE_SYNC_ON_WHEN_IDLE,
        .available_power_sources = ESP_ZB_AF_NODE_POWER_SOURCE_CONSTANT_POWER,
        .current_power_source = ESP_ZB_AF_NODE_POWER_SOURCE_CONSTANT_POWER,
        .current_power_source_level = ESP_ZB_AF_NODE_POWER_SOURCE_LEVEL_100_PERCENT-1};

    esp_zb_ep_list_t *esp_zb_ep_list = esp_zb_ep_list_create();
    esp_zb_cluster_list_t *esp_zb_cluster_list = esp_zb_zcl_cluster_list_create();

    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG();
    esp_zb_init(&zb_nwk_cfg);


    esp_zb_attribute_list_t *esp_zb_basic_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_BASIC);
    esp_zb_attribute_list_t *custom_cluster = esp_zb_zcl_attr_list_create(CUSTOM_CLUSTER_ID);


    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, &manufname[0]);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID, &power_source);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, &modelid[0]);

    // Custom cluster list
    esp_zb_custom_cluster_add_custom_attr(custom_cluster,0x0000,ESP_ZB_ZCL_ATTR_TYPE_S16,aclist,&temperaturebuffer[0]);
    esp_zb_custom_cluster_add_custom_attr(custom_cluster,0x0001,ESP_ZB_ZCL_ATTR_TYPE_S16,aclist,&temperaturebuffer[1]);
    esp_zb_custom_cluster_add_custom_attr(custom_cluster,0x0002,ESP_ZB_ZCL_ATTR_TYPE_S16,aclist,&temperaturebuffer[2]);
    esp_zb_custom_cluster_add_custom_attr(custom_cluster,0x0003,ESP_ZB_ZCL_ATTR_TYPE_S16,aclist,&temperaturebuffer[3]);
    #if TEMPERATURE_MODULES == 2
    esp_zb_custom_cluster_add_custom_attr(custom_cluster,0x0004,ESP_ZB_ZCL_ATTR_TYPE_S16,aclist,&temperaturebuffer[4]);
    esp_zb_custom_cluster_add_custom_attr(custom_cluster,0x0005,ESP_ZB_ZCL_ATTR_TYPE_S16,aclist,&temperaturebuffer[5]);
    esp_zb_custom_cluster_add_custom_attr(custom_cluster,0x0006,ESP_ZB_ZCL_ATTR_TYPE_S16,aclist,&temperaturebuffer[6]);
    esp_zb_custom_cluster_add_custom_attr(custom_cluster,0x0007,ESP_ZB_ZCL_ATTR_TYPE_S16,aclist,&temperaturebuffer[7]);
    #endif
    // end Custom cluster list

    esp_zb_cluster_list_add_identify_cluster(esp_zb_cluster_list, esp_zb_identify_cluster_create(&identify_cluster_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_basic_cluster(esp_zb_cluster_list, esp_zb_basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_custom_cluster(esp_zb_cluster_list,custom_cluster,ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);


    esp_zb_ep_list_add_ep(esp_zb_ep_list, esp_zb_cluster_list,endpoint_config);

    esp_zb_set_node_power_descriptor(node_power_desc);

    esp_zb_device_register(esp_zb_ep_list);
    esp_zb_core_action_handler_register(zb_action_handler);
    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_stack_main_loop();
}
