//
// Created by emeka.ariwodo on 3/16/2026.
//

#include "z2m.h"
#include "zcl/esp_zigbee_zcl_common.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "esp_log.h"
#include "esp_check.h"


char isactive[40];
int16_t temperaturebuffer[4] = {1300,1670,1020,1111};

typedef struct zbstring_s {
    uint8_t len;
    char data[];
} ESP_ZB_PACKED_STRUCT
zbstring_t;

static void update_tc_attr(uint16_t attr_id, int16_t value)
{
    esp_zb_lock_acquire(portMAX_DELAY);

    esp_zb_zcl_status_t st = esp_zb_zcl_set_attribute_val(
        10,                              // endpoint
        CUSTOM_CLUSTER_ID,               // 0xFF00
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        attr_id,                         // 0x0000..0x0003
        &value,
        false
    );

    esp_zb_lock_release();

    //ESP_LOGI("ZB", "set attr 0x%04X = %d status=%d", attr_id, value, st);
}

static void esp_app_temp_sensor_handler()
{
    /* Update temperature sensor measured value */
    esp_zb_lock_acquire(pdMS_TO_TICKS(10000));
    printf("Aquire lock\n");
    /*

    esp_zb_zcl_attr_t *a;

    a = esp_zb_zcl_get_attribute(
        pos,
        ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID
    );
    printf("measured attr=%p data=%p\n", a, a ? a->data_p : NULL);

    esp_zb_zcl_set_attribute_val(
        pos,
        ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID,
        &temperaturebuffer[pos],
        false);
        */
    esp_zb_lock_release();
}
static void configure_local_reporting_for_attr(uint16_t attr_id)
{
    static int16_t change = 1;

    esp_zb_zcl_config_report_record_t rec = {
        .direction = ESP_ZB_ZCL_REPORT_DIRECTION_SEND,
        .attributeID = attr_id,
        .attrType = ESP_ZB_ZCL_ATTR_TYPE_S16,
        .min_interval = 10,
        .max_interval = 3600,
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
        //temperature measurement message'
        esp_app_temp_sensor_handler();

    }


    /*
        typedef struct esp_zb_zcl_cmd_default_resp_message_s {
            esp_zb_zcl_cmd_info_t info;      !< The basic information of configuring report response message that refers to esp_zb_zcl_cmd_info_t
            uint8_t resp_to_cmd;             !< The field specifies the identifier of the received command to which this command is a response
            esp_zb_zcl_status_t status_code; !< The field specifies the nature of the error that was detected in the received command, refer to esp_zb_zcl_status_t
        } esp_zb_zcl_cmd_default_resp_message_t;

        typedef struct esp_zb_zcl_cmd_info_s {
        esp_zb_zcl_status_t status;       < The status of command, which can refer to  esp_zb_zcl_status_t
        esp_zb_zcl_frame_header_t header; < The command frame properties, which can refer to esp_zb_zcl_frame_field_t
        esp_zb_zcl_addr_t src_address;    < The struct of address contains short and ieee address, which can refer to esp_zb_zcl_addr_s
        uint16_t dst_address;             < The destination short address of command
        uint8_t src_endpoint;             < The source endpoint of command
        uint8_t dst_endpoint;             < The destination endpoint of command
        uint16_t cluster;                 < The cluster id for command
        uint16_t profile;                 < The application profile identifier
        esp_zb_zcl_command_t command;     < The properties of command
    } esp_zb_zcl_cmd_info_t;

        typedef struct esp_zb_zcl_report_attr_message_s {
            esp_zb_zcl_status_t status;       !< The status of the report attribute response, which can refer to esp_zb_zcl_status_t
            esp_zb_zcl_addr_t src_address;    !< The struct of address contains short and ieee address, which can refer to esp_zb_zcl_addr_s
            uint8_t src_endpoint;             !< The endpoint id which comes from report device
            uint8_t dst_endpoint;             !< The destination endpoint id
            uint16_t cluster;                 !< The cluster id that reported
            esp_zb_zcl_attribute_t attribute; !< The attribute entry of report response
        } esp_zb_zcl_report_attr_message_t;
     */

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

    esp_zb_ep_list_t *esp_zb_ep_list = esp_zb_ep_list_create();

    esp_zb_cluster_list_t *esp_zb_cluster_list = esp_zb_zcl_cluster_list_create();

    esp_zb_endpoint_config_t cfg = {
        .endpoint = 10,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_CUSTOM_ATTR_DEVICE_ID,
        .app_device_version = 0
    };
    esp_zb_identify_cluster_cfg_t identify_cluster_cfg = {
        .identify_time = 0,
    };
    // Custom mfg/model
    /*
    zbstring_t modelid;
    zbstring_t attributeName;
    zbstring_t manufname;
    modelid.len = 14;
    manufname.len = 9;
    attributeName.len = 15;
    memccpy(&modelid.data,"ESP32C6.Sensor",'\0',modelid.len);

    memccpy(&manufname.data,"Espressif",'\0',9);
    memccpy(&attributeName.data,"TemperatureData",'\0',attributeName.len);
    */
    char manufname[] = {9, 'E', 's', 'p', 'r', 'e', 's', 's', 'i', 'f'};
    char modelid[] = {14, 'E', 'S', 'P', '3', '2', 'C', '6', '.', 'S', 'e', 'n', 's', 'o', 'r'};
    char attributeName[] = {11,'T','e','m','p','e','r','a','t','u','r','e'};

    /* Initialize Zigbee stack */
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG();
    esp_zb_init(&zb_nwk_cfg);
    int16_t temp_min = INT16_MIN;
    int16_t temp_max = INT16_MAX;
    int16_t temp_tol = 10;


    esp_zb_attribute_list_t *esp_zb_basic_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_BASIC);
    esp_zb_attribute_list_t *custom_cluster = esp_zb_zcl_attr_list_create(CUSTOM_CLUSTER_ID);
    esp_zb_attribute_list_t *esp_zb_temp_sensor_attr_list = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT);

    uint8_t power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DC_SOURCE;
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, &manufname[0]);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID, &power_source);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, &modelid[0]);
    /*
    esp_zb_temperature_meas_cluster_add_attr(esp_zb_temp_sensor_attr_list, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID, &temperaturebuffer[0]);
    esp_zb_temperature_meas_cluster_add_attr(esp_zb_temp_sensor_attr_list, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MIN_VALUE_ID, &temp_min);
    esp_zb_temperature_meas_cluster_add_attr(esp_zb_temp_sensor_attr_list, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MAX_VALUE_ID, &temp_max);
    esp_zb_temperature_meas_cluster_add_attr(esp_zb_temp_sensor_attr_list, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_TOLERANCE_ID, &temp_tol);
    */

    esp_zb_zcl_attr_access_t aclist = ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING;
/*
    esp_zb_custom_cluster_add_custom_attr(custom_cluster,0x0000,ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING,
        ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY ,&attributeName);*/

    esp_zb_custom_cluster_add_custom_attr(custom_cluster,0x0000,ESP_ZB_ZCL_ATTR_TYPE_S16,aclist,&temperaturebuffer[0]);
    esp_zb_custom_cluster_add_custom_attr(custom_cluster,0x0001,ESP_ZB_ZCL_ATTR_TYPE_S16,aclist,&temperaturebuffer[1]);
    esp_zb_custom_cluster_add_custom_attr(custom_cluster,0x0002,ESP_ZB_ZCL_ATTR_TYPE_S16,aclist,&temperaturebuffer[2]);
    esp_zb_custom_cluster_add_custom_attr(custom_cluster,0x0003,ESP_ZB_ZCL_ATTR_TYPE_S16,aclist,&temperaturebuffer[3]);

    //esp_zb_zcl_set_manufacturer_attribute_val(10,CUSTOM_CLUSTER_ID,ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,0x1234,0x0000,&temperaturebuffer[0],false);
    //esp_zb_zcl_set_manufacturer_attribute_val(10,CUSTOM_CLUSTER_ID,ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,0x1234,0x0001,&temperaturebuffer[1],false);
    //esp_zb_zcl_set_manufacturer_attribute_val(10,CUSTOM_CLUSTER_ID,ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,0x1234,0x0002,&temperaturebuffer[2],false);
    //esp_zb_zcl_set_manufacturer_attribute_val(10,CUSTOM_CLUSTER_ID,ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,0x1234,0x0003,&temperaturebuffer[3],false);


    esp_zb_cluster_list_add_custom_cluster(esp_zb_cluster_list,custom_cluster,ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    //esp_zb_cluster_list_add_temperature_meas_cluster(esp_zb_cluster_list,esp_zb_temp_sensor_attr_list,ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_basic_cluster(esp_zb_cluster_list, esp_zb_basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(esp_zb_cluster_list, esp_zb_identify_cluster_create(&identify_cluster_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    esp_zb_ep_list_add_ep(esp_zb_ep_list, esp_zb_cluster_list, cfg);

    esp_zb_af_node_power_desc_t powerinfo = {
        .current_power_mode = ESP_ZB_AF_NODE_POWER_MODE_SYNC_ON_WHEN_IDLE,
        .available_power_sources = ESP_ZB_AF_NODE_POWER_SOURCE_CONSTANT_POWER,
        .current_power_source = ESP_ZB_AF_NODE_POWER_SOURCE_CONSTANT_POWER,
        .current_power_source_level = ESP_ZB_AF_NODE_POWER_SOURCE_LEVEL_100_PERCENT
    };

    esp_zb_device_register(esp_zb_ep_list);
    esp_zb_set_node_power_descriptor(powerinfo);
    esp_zb_core_action_handler_register(zb_action_handler);
    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_stack_main_loop();
}
