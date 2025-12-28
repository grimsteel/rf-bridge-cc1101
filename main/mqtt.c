/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/* MQTT over SSL Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "mqtt.h"
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "esp_log.h"
#include "event_queue.h"
#include "mqtt_client.h"
#include "portmacro.h"
#include <strings.h>
#include <sys/param.h>

#define MQTT_PREFIX "devices/rf_bridge_2/"
#define MQTT_SET_LIGHT_PREFIX MQTT_PREFIX "light_channel_"
#define MQTT_SET_LIGHT_SUFFIX "/set"
// devices/rf_bridge_2/light_channel_
#define MQTT_SET_LIGHT_TOPIC_LEN_PREFIX 34
// /set
#define MQTT_SET_LIGHT_TOPIC_LEN_SUFFIX 4
#define MQTT_SET_LIGHT_TOPIC_LEN (MQTT_SET_LIGHT_TOPIC_LEN_PREFIX + 1 + MQTT_SET_LIGHT_TOPIC_LEN_SUFFIX)

static const char *TAG = "mqtts_example";

extern const uint8_t isrgrootx1_pem_start[]   asm("_binary_isrgrootx1_pem_start");
extern const uint8_t isrgrootx1_pem_end[]   asm("_binary_isrgrootx1_pem_end");

extern const char discovery_start[]   asm("_binary_discovery_payload_json_start");
extern const char discovery_end[]   asm("_binary_discovery_payload_json_end");

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
  //ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
  esp_mqtt_event_handle_t event = event_data;
  esp_mqtt_client_handle_t client = event->client;
  QueueHandle_t recv_queue = (QueueHandle_t) handler_args;

  switch ((esp_mqtt_event_id_t)event_id) {
  case MQTT_EVENT_CONNECTED:
    // subscribe to topics
    esp_mqtt_client_subscribe(client, MQTT_PREFIX "onboard_led/set", 0);
    esp_mqtt_client_subscribe(client, MQTT_PREFIX "light_channel_e/set", 0);
    esp_mqtt_client_subscribe(client, MQTT_PREFIX "light_channel_a/set", 0);

    ESP_LOGI(TAG, "Connected");
    break;

  case MQTT_EVENT_DATA:
    // check equal to 39 chars
    if (event->topic_len == MQTT_SET_LIGHT_TOPIC_LEN && strncasecmp(event->topic, MQTT_SET_LIGHT_PREFIX, MQTT_SET_LIGHT_TOPIC_LEN_PREFIX) == 0) {
        char light_id = event->topic[MQTT_SET_LIGHT_TOPIC_LEN_PREFIX];
        bool turn_on = event->data_len == 2 && strncasecmp(event->data, "on", 2);
        event_queue_message_t evt = {
            .data.mqtt_message.light_id = light_id,
            .data.mqtt_message.turn_on = turn_on,
            .type = EVENT_QUEUE_MESSAGE_MQTT
        };
        xQueueSend(recv_queue, &evt, portMAX_DELAY);
    }
    break;

  case MQTT_EVENT_ERROR:
    if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
      ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
      ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
      ESP_LOGI(TAG, "Last captured errno : %d (%s)",  event->error_handle->esp_transport_sock_errno,
               strerror(event->error_handle->esp_transport_sock_errno));
    } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
      ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
    } else {
      ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
    }

    break;
  default: break;
  }

}

esp_mqtt_client_handle_t mqtt_app_start(QueueHandle_t recv_queue)
{
  const esp_mqtt_client_config_t mqtt_cfg = {
    .credentials = {
      .username = CONFIG_MQTT_USERNAME,
      .authentication = {
        .password = CONFIG_MQTT_PASSWORD
      }
    },
    .broker = {
      .address.uri = CONFIG_MQTT_BROKER_ADDRESS,
      .verification = {
        .certificate = (const char*) isrgrootx1_pem_start,
        .common_name = CONFIG_MQTT_CERT_COMMON_NAME
      }
    },
  };

  assert(recv_queue);

  esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
  /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
  esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, recv_queue);
  esp_mqtt_client_start(client);

  // MQTT discovery
  esp_mqtt_client_publish(client, "homeassistant/device/rf-bridge-2/config", discovery_start, 0, 0, 0);

  return client;
}
