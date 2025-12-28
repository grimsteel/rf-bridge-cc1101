#pragma once

#include <driver/rmt_rx.h>
#include <freertos/FreeRTOS.h>
#include "rf_light_encoder.h"

// each actual message is only 16 symbols, so 64 is plenty
#define SYMBOL_BUFFER_SIZE 64
#define PARSED_MESSAGE_QUEUE_LENGTH 4

typedef struct {
  QueueHandle_t parsed_message_queue;
  rmt_channel_handle_t channel;
  rmt_symbol_word_t symbols[SYMBOL_BUFFER_SIZE];
  rmt_receive_config_t config;
} rf_light_rx_data_t;

esp_err_t rf_light_initialize_rx(gpio_num_t rx_gpio_num, rf_light_rx_data_t* rx_data);
