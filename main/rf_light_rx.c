#include "rf_light_rx.h"

#include <driver/rmt_rx.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include "esp_check.h"
#include "event_queue.h"

#define TAG "RF Light RMT RX"

// Debug tool to print a whole received message
static void print_rmt_frame(size_t num, rmt_symbol_word_t* symbols) {
  fprintf(stderr, "Received Raw: ");

  for (int i =0 ; i < num; i++) {
    fprintf(stderr, "%d, -%d, ", symbols[i].duration0, symbols[i].duration1);
  }

  fprintf(stderr, "\n");
}

// timing definitions for our protocol ar ein encoder.h
#define RF_LIGHT_DECODE_MARGIN 200

static inline bool rf_light_check_in_range(uint32_t signal_duration, uint32_t spec_duration)
{
  // mul by 2 for clock
    return (signal_duration*2 < (spec_duration + RF_LIGHT_DECODE_MARGIN)) &&
           (signal_duration*2 > (spec_duration - RF_LIGHT_DECODE_MARGIN));
}

/**
 * @brief Check whether a RMT symbol represents RF_LIGHT logic zero
 */
static bool rf_light_parse_logic0(rmt_symbol_word_t *rmt_rf_light_symbols, bool last)
{
    return rf_light_check_in_range(rmt_rf_light_symbols->duration0, RF_LIGHT_PAYLOAD_ZERO_DURATION_0) &&
      (last || rf_light_check_in_range(rmt_rf_light_symbols->duration1, RF_LIGHT_PAYLOAD_ZERO_DURATION_1));
}

/**
 * @brief Check whether a RMT symbol represents RF_LIGHT logic one
 */
static bool rf_light_parse_logic1(rmt_symbol_word_t *rmt_rf_light_symbols, bool last)
{
    return rf_light_check_in_range(rmt_rf_light_symbols->duration0, RF_LIGHT_PAYLOAD_ONE_DURATION_0) &&
      // don't check duration1 if this is the last bit
      (last || rf_light_check_in_range(rmt_rf_light_symbols->duration1, RF_LIGHT_PAYLOAD_ONE_DURATION_1));
}

// Parse an entire frame for any messages within
static void parse_rmt_frame(size_t num_items, rmt_symbol_word_t* x, QueueHandle_t parsed_message_queue, BaseType_t* high_task_wakeup) {
  int bit = 0;
  rf_light_message_t message = 0;
  rf_light_message_t previous_message = 0;

  for (size_t i = 0; i < num_items; i++) {
    // test for logic 0 or logic 1
    if (rf_light_parse_logic0(&x[i], bit == 15)) {
      message &= ~(1 << (bit++));
    } else if (rf_light_parse_logic1(&x[i], bit == 15)) {
      message |= (1 << (bit++));
    } else {
      if (bit > 2) {
        ESP_LOGW(TAG, "Failed to receive on bit %d", bit);
      }

      // fail
      bit = 0;
    }

    if (bit == 16) {
      // done
      bit = 0;
      // many repeat messages
      if (message != previous_message) {
        //ESP_LOGW(TAG, "Successfully received message %04X", message);

        event_queue_message_t msg = {
            .data.rf_light_message = message,
            .type = EVENT_QUEUE_MESSAGE_RF_LIGHT
        };

        // send this to the queue
        xQueueSendFromISR(parsed_message_queue, &msg, high_task_wakeup);
      }

      previous_message = message;
    }
  }
}

static bool rf_light_rx_done_callback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_data)
{
  BaseType_t high_task_wakeup = pdFALSE;

  // print the frame
  //print_rmt_frame(edata->num_symbols, edata->received_symbols);

  rf_light_rx_data_t* rx_data = (rf_light_rx_data_t*) user_data;

  // parse messages and send to queue
  parse_rmt_frame(edata->num_symbols, edata->received_symbols, rx_data->parsed_message_queue, &high_task_wakeup);

  // start receiving again
  ESP_ERROR_CHECK(rmt_receive(rx_data->channel, rx_data->symbols, sizeof(rx_data->symbols), &rx_data->config));

  return high_task_wakeup == pdTRUE;
}

esp_err_t rf_light_initialize_rx(gpio_num_t rx_gpio_num, rf_light_rx_data_t* rx_data) {
  // Initialize RMT channel
  ESP_LOGI(TAG, "Initialize RMT channel");
  rmt_rx_channel_config_t rx_channel_cfg = {
    .clk_src = RMT_CLK_SRC_DEFAULT,
    .resolution_hz = 1000000 / 2, // 1 tick = 2us
    .mem_block_symbols = SYMBOL_BUFFER_SIZE, // Store 64 symbols
    .gpio_num = rx_gpio_num
  };
  ESP_RETURN_ON_ERROR(rmt_new_rx_channel(&rx_channel_cfg, &rx_data->channel), TAG, "Failed to initialize channel");

  // Initialize the data queue
  rx_data->parsed_message_queue = xQueueCreate(PARSED_MESSAGE_QUEUE_LENGTH, sizeof(event_queue_message_t));
  assert(rx_data->parsed_message_queue);

  // Setup the callbacks
  rmt_rx_event_callbacks_t cbs = {
    .on_recv_done = rf_light_rx_done_callback,
  };
  // pass the rx_data struct in the data param
  ESP_RETURN_ON_ERROR(rmt_rx_register_event_callbacks(rx_data->channel, &cbs, rx_data), TAG, "Failed to add RX callback");

  // Timing thresholds for the RMT driver

  // 3 us minimum time (the actual value is bigger, but the driver doesn't go higher)
  rx_data->config.signal_range_min_ns = 3 * 1000;
  // 6000 us maximum time (max delay between messages)
  rx_data->config.signal_range_max_ns =  6000 * 1000;

  // Enable the channel and begin receiving
  ESP_RETURN_ON_ERROR(rmt_enable(rx_data->channel), TAG, "Failed to enable channel");
  esp_err_t err = rmt_receive(rx_data->channel, rx_data->symbols, sizeof(rx_data->symbols), &rx_data->config);
  ESP_RETURN_ON_ERROR(err, TAG, "Failed to begin receiving: %d", err);

  return ESP_OK;
}
