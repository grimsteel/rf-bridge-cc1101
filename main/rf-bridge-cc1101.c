#include <stdio.h>
#include "event_queue.h"
#include "freertos/idf_additions.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
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

  // init RMT receiver and start RX
  rf_light_rx_data_t rx_data = {0};
  ESP_ERROR_CHECK(rf_light_initialize_rx(GPIO_NUM_33, &rx_data));
  rf_light_tx_t tx = {0};
  ESP_ERROR_CHECK(rf_light_initialize_tx(&tx, GPIO_NUM_33));

  esp_mqtt_client_handle_t mqtt = mqtt_app_start(rx_data.parsed_message_queue);

  ESP_ERROR_CHECK(cc1101_start_rx(cc1101));
  //ESP_ERROR_CHECK(cc1101_start_tx(cc1101));
  //
  cc1101_debug_print_regs(cc1101);

  event_queue_message_t message_payload;

  gpio_set_level(GPIO_NUM_14, 0);

  bool on = false;

  while (1) {
    /*ESP_LOGI(TAG, "send tx");
    for (int i = 0; i < 10; i++) {
        ESP_ERROR_CHECK(rf_light_tx_send(&tx, on ? 0x8CAA : 0x4CAA));
        vTaskDelay(1);
    }
    on = !on;
    vTaskDelay(5000 / portTICK_PERIOD_MS);*/
    // wait for RX done signal
    if (xQueueReceive(rx_data.parsed_message_queue, &message_payload, portMAX_DELAY)) {
        if (message_payload.type == EVENT_QUEUE_MESSAGE_RF_LIGHT) {
            ESP_LOGI(TAG, "Received %04X", message_payload.data.rf_light_message);
            switch (message_payload.data.rf_light_message) {
            case 0x8CAA:
                esp_mqtt_client_publish(mqtt, "devices/rf_bridge_2/light_channel_e/state", "ON", 0, 0, 0);
                break;
            case 0x4CAA:
                esp_mqtt_client_publish(mqtt, "devices/rf_bridge_2/light_channel_e/state", "OFF", 0, 0, 0);
                break;

            case 0x88AA:
                esp_mqtt_client_publish(mqtt, "devices/rf_bridge_2/light_channel_a/state", "ON", 0, 0, 0);
                break;
            case 0x48AA:
                esp_mqtt_client_publish(mqtt, "devices/rf_bridge_2/light_channel_a/state", "OFF", 0, 0, 0);
                break;
            }
        } else if (message_payload.type == EVENT_QUEUE_MESSAGE_MQTT) {
            ESP_LOGI(TAG, "Received %c %b", message_payload.data.mqtt_message.light_id, message_payload.data.mqtt_message.turn_on);
            ESP_ERROR_CHECK(cc1101_start_tx(cc1101));
            for (int i = 0; i < 10; i++) {
                ESP_ERROR_CHECK(rf_light_tx_send(&tx, message_payload.data.mqtt_message.turn_on ? 0x8CAA : 0x4CAA));
                vTaskDelay(1);
            }
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            ESP_ERROR_CHECK(cc1101_start_rx(cc1101));
        }
    }
  }
}
