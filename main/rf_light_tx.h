#pragma once

#include <driver/rmt_tx.h>
#include <freertos/FreeRTOS.h>
#include "driver/gpio.h"
#include "driver/rmt_types.h"
#include "esp_err.h"
#include "rf_light_encoder.h"

typedef struct {
    rmt_encoder_handle_t encoder;
    rmt_channel_handle_t channel;
} rf_light_tx_t;

esp_err_t rf_light_initialize_tx(rf_light_tx_t *rf_light_tx, gpio_num_t tx_gpio_num);
esp_err_t rf_light_tx_send(rf_light_tx_t *rf_light_tx, rf_light_message_t message);
esp_err_t rf_light_tx_free(rf_light_tx_t *rf_light_tx);
