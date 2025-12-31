#include <stdio.h>
#include "esp_system.h"
#include "event_queue.h"
#include "freertos/idf_additions.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "rf_light_encoder.h"
#include "rf_light_tx.h"
#include "wifi.h"
#include "mqtt.h"
#include "cc1101_setup.h"
#include "rf_light_rx.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "driver/gpio.h"

#define TAG "rf-bridge-cc1101"

void app_main(void)
{
    ESP_LOGI(TAG, "last reset reason %d", esp_reset_reason());

  // general ESP32 initializations
  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  ESP_ERROR_CHECK(gpio_install_isr_service(0));

  // Onboard LED
  gpio_config_t led_config = {
    .intr_type = GPIO_INTR_DISABLE,
    .mode = GPIO_MODE_OUTPUT,
    .pin_bit_mask = (1ULL << GPIO_NUM_14),
    .pull_down_en = 0,
    .pull_up_en = 0
  };
  gpio_config(&led_config);

  // turn on when initializing
  gpio_set_level(GPIO_NUM_14, 1);

  // Wi-Fi
  initialize_wifi();

  // init cc1101
  cc1101_device_t* cc1101;
  ESP_ERROR_CHECK(init_cc1101(&cc1101));

  QueueHandle_t message_queue = xQueueCreate(PARSED_MESSAGE_QUEUE_LENGTH, sizeof(event_queue_message_t));

  // init RMT receiver and start RX
  rf_light_rx_data_t rx_data = {0};
  rx_data.parsed_message_queue = message_queue;
  ESP_ERROR_CHECK(rf_light_initialize_rx(GPIO_NUM_9, &rx_data));
  rf_light_tx_t tx = {0};
  ESP_ERROR_CHECK(rf_light_initialize_tx(&tx, GPIO_NUM_8));

  esp_mqtt_client_handle_t mqtt = mqtt_app_start(message_queue);

  ESP_ERROR_CHECK(cc1101_start_rx(cc1101));
  cc1101_debug_print_regs(cc1101);

  event_queue_message_t message_payload;

  rf_light_payload_t decoded_message;

  gpio_set_level(GPIO_NUM_14, 0);

  bool on = false;

  while (1) {
    // wait for RX done signal
    if (xQueueReceive(message_queue, &message_payload, portMAX_DELAY)) {
        if (message_payload.type == EVENT_QUEUE_MESSAGE_RF_LIGHT) {

            if (decode_rf_light_payload(message_payload.data.rf_light_message, &decoded_message)) {
                // error
                ESP_LOGW(TAG, "Received invalid RF Light message: %04X", message_payload.data.rf_light_message);
            } else {
                ESP_LOGI(TAG, "Received RF light message | Channel: %c | On: %d", decoded_message.channel, decoded_message.on);

                char topic[42];
                snprintf(topic, 42, "devices/rf_bridge_2/light_channel_%c/state", decoded_message.channel);

                esp_mqtt_client_publish(mqtt, topic, decoded_message.on ? "ON" : "OFF", 0, 0, 0);
            }
        } else if (message_payload.type == EVENT_QUEUE_MESSAGE_MQTT) {
            ESP_LOGI(TAG, "Received MQTT message | Channel: %c | On: %d", message_payload.data.mqtt_message.light_id, message_payload.data.mqtt_message.turn_on);
            decoded_message.channel = message_payload.data.mqtt_message.light_id;
            decoded_message.on = message_payload.data.mqtt_message.turn_on;
            // encode
            rf_light_message_t message = encode_rf_light_payload(&decoded_message);
            ESP_LOGI(TAG, "Sending message %04X", message);
            ESP_ERROR_CHECK(cc1101_start_tx(cc1101));
            ESP_ERROR_CHECK(rf_light_tx_send(&tx, message));
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            ESP_ERROR_CHECK(cc1101_start_rx(cc1101));
        }
    }
  }
}
