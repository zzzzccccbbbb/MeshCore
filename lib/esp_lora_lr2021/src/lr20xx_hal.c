/*!
 * \file      lr20xx_hal.c
 * \brief     LR20XX HAL for PlatformIO/Arduino ESP32 (W20 LR2021)
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "lr20xx_hal.h"
#include "apps_common.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "LR20XX_HAL";

extern void radio_on_dio_irq(void *context);

static spi_device_handle_t s_spiHandle = NULL;
static SemaphoreHandle_t s_spiMutex = NULL;
static bool s_isrAttached = false;

static bool lr20xx_spi_lock(void)
{
    if (!s_spiMutex) {
        return true;
    }
    return xSemaphoreTake(s_spiMutex, pdMS_TO_TICKS(5000)) == pdTRUE;
}

static void lr20xx_spi_unlock(void)
{
    if (s_spiMutex) {
        xSemaphoreGive(s_spiMutex);
    }
}

static lr20xx_hal_status_t lr20xx_spi_transfer_raw(const uint8_t *tx, uint8_t *rx, size_t len)
{
    if (len == 0) {
        return LR20XX_HAL_STATUS_OK;
    }
    if (!s_spiHandle) {
        return LR20XX_HAL_STATUS_ERROR;
    }
    if (!lr20xx_spi_lock()) {
        return LR20XX_HAL_STATUS_ERROR;
    }

    spi_transaction_t trans = {0};
    trans.length = len * 8;
    trans.tx_buffer = tx;
    trans.rx_buffer = rx;

    esp_err_t ret = spi_device_transmit(s_spiHandle, &trans);
    lr20xx_spi_unlock();

    return (ret == ESP_OK) ? LR20XX_HAL_STATUS_OK : LR20XX_HAL_STATUS_ERROR;
}

static void IRAM_ATTR lr20xx_irq_isr(void *arg)
{
    (void)arg;
    radio_on_dio_irq(NULL);
}

void lr20xx_hal_wait_on_busy(const void *radio)
{
    (void)radio;
    uint32_t timeout = 1000;
    while (gpio_get_level((gpio_num_t)LORA_BUSY_PIN) == 1 && timeout--) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    if (timeout == 0) {
        ESP_LOGE(TAG, "Busy timeout!");
    }
}

lr20xx_hal_status_t lr20xx_hal_init(void)
{
    esp_err_t ret;

    if (!s_spiMutex) {
        s_spiMutex = xSemaphoreCreateMutex();
        if (!s_spiMutex) {
            return LR20XX_HAL_STATUS_ERROR;
        }
    }

    gpio_config_t io = {};
    io.mode = GPIO_MODE_OUTPUT;
    io.intr_type = GPIO_INTR_DISABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.pull_up_en = GPIO_PULLUP_DISABLE;

    /* RST is always ours. Do NOT gpio_config CS after SPI owns it — that steals
     * the pin from the SPI master and subsequent reads return 0xFF (version 255.255). */
    io.pin_bit_mask = (1ULL << LORA_RST_PIN);
    gpio_config(&io);
    gpio_set_level((gpio_num_t)LORA_RST_PIN, 1);

    io.pin_bit_mask = (1ULL << LORA_BUSY_PIN);
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io);

    io.pin_bit_mask = (1ULL << LORA_IRQ_PIN);
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_DISABLE;
    io.pull_down_en = GPIO_PULLDOWN_ENABLE;
    io.intr_type = GPIO_INTR_POSEDGE;
    gpio_config(&io);

    if (!s_isrAttached) {
        ret = gpio_install_isr_service(0);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "ISR service install failed");
            return LR20XX_HAL_STATUS_ERROR;
        }
        ret = gpio_isr_handler_add((gpio_num_t)LORA_IRQ_PIN, lr20xx_irq_isr, NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "ISR handler add failed");
            return LR20XX_HAL_STATUS_ERROR;
        }
        s_isrAttached = true;
    }

    if (!s_spiHandle) {
        /* Drop stale SPI2 (wrong pins from a prior owner → version 255.255). */
        (void)spi_bus_free(SPI2_HOST);
        gpio_reset_pin((gpio_num_t)LORA_SCLK_PIN);
        gpio_reset_pin((gpio_num_t)LORA_MOSI_PIN);
        gpio_reset_pin((gpio_num_t)LORA_MISO_PIN);
        gpio_reset_pin((gpio_num_t)LORA_CS_PIN);

        /* Idle CS high before SPI driver claims the pin. */
        io.mode = GPIO_MODE_OUTPUT;
        io.intr_type = GPIO_INTR_DISABLE;
        io.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io.pull_up_en = GPIO_PULLUP_DISABLE;
        io.pin_bit_mask = (1ULL << LORA_CS_PIN);
        gpio_config(&io);
        gpio_set_level((gpio_num_t)LORA_CS_PIN, 1);

        spi_bus_config_t buscfg = {
            .miso_io_num = LORA_MISO_PIN,
            .mosi_io_num = LORA_MOSI_PIN,
            .sclk_io_num = LORA_SCLK_PIN,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 4096,
        };
        ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
            return LR20XX_HAL_STATUS_ERROR;
        }
        ESP_LOGI(TAG, "SPI2 SCK=%d MOSI=%d MISO=%d CS=%d", LORA_SCLK_PIN, LORA_MOSI_PIN, LORA_MISO_PIN,
                 LORA_CS_PIN);

        spi_device_interface_config_t devcfg = {
            .clock_speed_hz = 2 * 1000 * 1000, /* 2 MHz: more tolerant on first bring-up */
            .mode = 0,
            .spics_io_num = LORA_CS_PIN,
            .queue_size = 4,
        };
        ret = spi_bus_add_device(SPI2_HOST, &devcfg, &s_spiHandle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SPI device add failed: %s", esp_err_to_name(ret));
            return LR20XX_HAL_STATUS_ERROR;
        }
    }

    ESP_LOGI(TAG, "LR20xx HAL initialized, IRQ GPIO%d", LORA_IRQ_PIN);
    return LR20XX_HAL_STATUS_OK;
}

lr20xx_hal_status_t lr20xx_hal_reset(const void *context)
{
    (void)context;
    gpio_set_level((gpio_num_t)LORA_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(200));
    gpio_set_level((gpio_num_t)LORA_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
    return LR20XX_HAL_STATUS_OK;
}

lr20xx_hal_status_t lr20xx_hal_deinit(void)
{
    if (s_isrAttached) {
        gpio_isr_handler_remove((gpio_num_t)LORA_IRQ_PIN);
        s_isrAttached = false;
    }
    if (s_spiHandle) {
        spi_bus_remove_device(s_spiHandle);
        s_spiHandle = NULL;
        spi_bus_free(SPI2_HOST);
    }
    /* Restore CS/RST as plain GPIO for RadioLib software-CS path */
    gpio_config_t io = {};
    io.mode = GPIO_MODE_OUTPUT;
    io.pin_bit_mask = (1ULL << LORA_CS_PIN) | (1ULL << LORA_RST_PIN);
    gpio_config(&io);
    gpio_set_level((gpio_num_t)LORA_CS_PIN, 1);
    gpio_set_level((gpio_num_t)LORA_RST_PIN, 1);
    return LR20XX_HAL_STATUS_OK;
}

lr20xx_hal_status_t lr20xx_hal_wakeup(const void *context)
{
    return lr20xx_hal_reset(context);
}

lr20xx_hal_status_t lr20xx_hal_write(const void *context, const uint8_t *command, const uint16_t command_length,
                                     const uint8_t *data, const uint16_t data_length)
{
    (void)context;
    lr20xx_hal_wait_on_busy(context);

    uint16_t total_length = command_length + data_length;
    if (total_length == 0) {
        return LR20XX_HAL_STATUS_OK;
    }

    uint8_t *tx_buffer = (uint8_t *)malloc(total_length);
    if (!tx_buffer) {
        return LR20XX_HAL_STATUS_ERROR;
    }

    memcpy(tx_buffer, command, command_length);
    if (data && data_length > 0) {
        memcpy(tx_buffer + command_length, data, data_length);
    }

    lr20xx_hal_status_t st = lr20xx_spi_transfer_raw(tx_buffer, NULL, total_length);
    free(tx_buffer);
    vTaskDelay(pdMS_TO_TICKS(2));
    return st;
}

lr20xx_hal_status_t lr20xx_hal_read(const void *context, const uint8_t *command, const uint16_t command_length,
                                    uint8_t *data, const uint16_t data_length)
{
    if (data_length == 0) {
        return LR20XX_HAL_STATUS_OK;
    }

    lr20xx_hal_status_t st = lr20xx_spi_transfer_raw(command, NULL, command_length);
    if (st != LR20XX_HAL_STATUS_OK) {
        return st;
    }

    lr20xx_hal_wait_on_busy(context);

    uint16_t total_dummy = data_length + 2;
    uint8_t *tx_dummy = (uint8_t *)calloc(total_dummy, 1);
    uint8_t *rx_buf = (uint8_t *)malloc(total_dummy);
    if (!tx_dummy || !rx_buf) {
        free(tx_dummy);
        free(rx_buf);
        return LR20XX_HAL_STATUS_ERROR;
    }

    st = lr20xx_spi_transfer_raw(tx_dummy, rx_buf, total_dummy);
    if (st == LR20XX_HAL_STATUS_OK) {
        memcpy(data, rx_buf + 2, data_length);
    }

    free(tx_dummy);
    free(rx_buf);
    return st;
}

lr20xx_hal_status_t lr20xx_hal_direct_read_fifo(const void *radio, const uint8_t *command, const uint16_t command_length,
                                                uint8_t *data, const uint16_t data_length)
{
    lr20xx_hal_wait_on_busy(radio);
    if (data_length == 0) {
        return LR20XX_HAL_STATUS_OK;
    }

    uint16_t total_len = command_length + data_length;
    uint8_t *tx_buf = (uint8_t *)calloc(total_len, 1);
    uint8_t *rx_buf = (uint8_t *)malloc(total_len);
    if (!tx_buf || !rx_buf) {
        free(tx_buf);
        free(rx_buf);
        return LR20XX_HAL_STATUS_ERROR;
    }

    memcpy(tx_buf, command, command_length);
    lr20xx_hal_status_t st = lr20xx_spi_transfer_raw(tx_buf, rx_buf, total_len);
    if (st == LR20XX_HAL_STATUS_OK) {
        memcpy(data, rx_buf + command_length, data_length);
    }

    free(tx_buf);
    free(rx_buf);
    vTaskDelay(pdMS_TO_TICKS(2));
    return st;
}

lr20xx_hal_status_t lr20xx_hal_direct_read(const void *radio, uint8_t *data, const uint16_t data_length)
{
    lr20xx_hal_wait_on_busy(radio);
    if (data_length == 0) {
        return LR20XX_HAL_STATUS_OK;
    }

    uint8_t *tx_dummy = (uint8_t *)calloc(data_length, 1);
    if (!tx_dummy) {
        return LR20XX_HAL_STATUS_ERROR;
    }

    lr20xx_hal_status_t st = lr20xx_spi_transfer_raw(tx_dummy, data, data_length);
    free(tx_dummy);
    vTaskDelay(pdMS_TO_TICKS(2));
    return st;
}
