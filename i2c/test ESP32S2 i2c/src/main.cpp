#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#define I2C_SLAVE_NUM I2C_NUM_0
#define I2C_SLAVE_SDA 8
#define I2C_SLAVE_SCL 9
#define I2C_SLAVE_ADDR 0x08
#define BUF_LEN 128

void app_main(void)
{
    i2c_config_t conf_slave = {
        .mode = I2C_MODE_SLAVE,
        .sda_io_num = I2C_SLAVE_SDA,
        .scl_io_num = I2C_SLAVE_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .slave = {
            .slave_addr = I2C_SLAVE_ADDR,
            .maximum_speed = 100000
        }
    };
    i2c_param_config(I2C_SLAVE_NUM, &conf_slave);
    i2c_driver_install(I2C_SLAVE_NUM, I2C_MODE_SLAVE, BUF_LEN, BUF_LEN, 0);

    uint8_t data[BUF_LEN];

    while (1) {
        int len = i2c_slave_read_buffer(I2C_SLAVE_NUM, data, BUF_LEN, 100 / portTICK_PERIOD_MS);
        if (len > 0) {
            ESP_LOGI("I2C_SLAVE", "Recibido %d bytes: %.*s", len, len, data);

            // Responder al master si se necesita
            const char *resp = "ACK";
            i2c_slave_write_buffer(I2C_SLAVE_NUM, (uint8_t*)resp, strlen(resp), 100 / portTICK_PERIOD_MS);
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
