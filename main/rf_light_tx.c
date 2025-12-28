#include "rf_light_tx.h"
#include "driver/rmt_common.h"
#include "driver/rmt_tx.h"
#include "rf_light_encoder.h"
#include "rf_light_rx.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <stdbool.h>
#include "esp_check.h"

#define TAG "RF Light RMT TX"

esp_err_t rf_light_initialize_tx(rf_light_tx_t* rf_light_tx, gpio_num_t tx_gpio_num) {
  // Initialize RMT channel
  ESP_LOGI(TAG, "Initialize RMT channel");
  rmt_tx_channel_config_t tx_channel_cfg = {
    .clk_src = RMT_CLK_SRC_DEFAULT,
    .resolution_hz = 1000000 / 2, // 1 tick = 2us
    .mem_block_symbols = SYMBOL_BUFFER_SIZE, // Store 64 symbols
    .trans_queue_depth = 4,
    .gpio_num = tx_gpio_num,
    .flags.invert_out = false,
  };
  ESP_RETURN_ON_ERROR(rmt_new_tx_channel(&tx_channel_cfg, &rf_light_tx->channel), TAG, "Failed to initialize channel");
  ESP_RETURN_ON_ERROR(rmt_enable(rf_light_tx->channel), TAG, "Failed to enable channel");

  // INitialize encoder
  ESP_RETURN_ON_ERROR(rf_light_encoder_new(&rf_light_tx->encoder), TAG, "Failed to initialize encoder");

  return ESP_OK;
}

esp_err_t rf_light_tx_send(rf_light_tx_t *rf_light_tx, rf_light_message_t message) {
    rmt_transmit_config_t transmit_config = {
        .loop_count = 0,
    };
    ESP_RETURN_ON_ERROR(rmt_transmit(rf_light_tx->channel, rf_light_tx->encoder, &message, sizeof(rf_light_message_t), &transmit_config), TAG, "Failed to send tx");
    return ESP_OK;
}
esp_err_t rf_light_tx_free(rf_light_tx_t *rf_light_tx) {
    if (rf_light_tx->channel != NULL) ESP_RETURN_ON_ERROR(rmt_del_channel(rf_light_tx->channel), TAG, "Failed to free channel");
    if (rf_light_tx->encoder != NULL) ESP_RETURN_ON_ERROR(rmt_del_encoder(rf_light_tx->encoder), TAG, "Failed to free encoder");
    return ESP_OK;
}
