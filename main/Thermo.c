#include <stdio.h>
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <driver/gpio.h>
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include <math.h>
#include "CN0391.h"
#include "esp_zigbee_core.h"
#include "nvs_flash.h"
#include "z2m.c"

esp_zb_ieee_addr_t extended_pan_id;
uint16_t pan_id;


#define SPI_MASTER_MOSI_IO 20
#define SPI_MASTER_MISO_IO 21
#define SPI_MASTER_SCLK_IO 22
#define SPI_MAST_CS_IO 19
#define SPI_TIMEOUT_MS 2000

#define DEVICE_ID 0


#define A (3.9083*0.001)
#define B (-5.775*pow(10, -7))


/*
 * Vtc = Vtc_read + Vcj
 * the cold junction temperature is calculated
 * Vcj = a0 + a1 T + a2 T^2 + a3 T^3 etc
 *
 *
 *
 *
 *
 */

const char *TAG = "Thermo";

void SPI_transaction_send(spi_device_handle_t* dev, uint8_t *data, uint16_t size)
{
    esp_err_t err;
    spi_transaction_t t;
    memset(&t, 0, sizeof(spi_transaction_t));
    t.length = 8 * size;
    t.tx_buffer = data;
    t.user = (void*)1;
    err = spi_device_polling_transmit(*dev,&t);
    assert(err == ESP_OK);
}
void SPI_transaction_read(spi_device_handle_t* dev, uint8_t *data, uint16_t size)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(spi_transaction_t));
    t.flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA;

}

static void log_nwk_info(const char *status_string)
{
    esp_zb_ieee_addr_t extended_pan_id;
    esp_zb_get_extended_pan_id(extended_pan_id);
    ESP_LOGI(TAG, "%s (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, "
                  "Channel:%d, Short Address: 0x%04hx)", status_string,
                  extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                  extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                  esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
}

void spi_PT_callback(spi_transaction_t *t)
{

}

void init_spi(spi_device_handle_t* spi)
{
    spi_bus_config_t bus_config = {
        .miso_io_num = SPI_MASTER_MISO_IO,
        .sclk_io_num = SPI_MASTER_SCLK_IO,
        .mosi_io_num = SPI_MASTER_MOSI_IO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    spi_device_interface_config_t spi_device = {
        .mode = 3,
        .spics_io_num = SPI_MAST_CS_IO,
        .clock_speed_hz = 10000000,
        .queue_size = 4,
    };

    ESP_ERROR_CHECK_WITHOUT_ABORT(spi_bus_initialize(SPI2_HOST,&bus_config,SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK_WITHOUT_ABORT(spi_bus_add_device(SPI2_HOST, &spi_device, spi));
    ESP_LOGI(TAG, "SPI initialization complete \n\n");
}

void init_items(spi_device_handle_t* spi)
{
    ESP_LOGI(TAG, "Initializing Thermo \n");
    init_spi(spi);
}
void print_chip_info(void)
{
    uint32_t flash_size;
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    ESP_LOGI("This is %s chip with %d CPU core(s), %s%s%s%s, \n",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;


    printf("silicon revision v%d.%d, ", major_rev, minor_rev);
    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("Get flash size failed");
    }
}
void esp_task_get_Temp_data_loop()
{
    while(true)
    {
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        CN0391_set_data();
        //CN0391_display_data();
        //printf(" \n\n\n\n ");
    }
}
void esp_task_SPI_init()
{
    spi_device_handle_t spidev;
    init_items(&spidev);
    CN0391_init(&spidev);
    esp_task_get_Temp_data_loop();
}
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_s)
{
    /*`
    * typedef struct esp_zb_app_signal_s {
    uint32_t *p_app_signal;   !< Application pointer signal type, refer to esp_zb_app_signal_type_t
    esp_err_t esp_err_status; !< The error status of the each signal event, refer to esp_err_t
    } esp_zb_app_signal_t;
     */
    ESP_LOGI("ESP ZB SIGNAL RECEIVED", "Signal Type: %lx \n error status: %ll", signal_s->p_app_signal, signal_s->esp_err_status);

}

void app_main(void)
{
    TaskHandle_t xSpi_handle = NULL;
    ESP_ERROR_CHECK(nvs_flash_init());
    print_chip_info();
    xTaskCreate(esp_task_SPI_init, "InitializeSPI", 4096, NULL, 5, &xSpi_handle);
    xTaskCreate(esp_zb_task,"zigbeeTask",4096,NULL,5,NULL);

   // esp_restart();

}
