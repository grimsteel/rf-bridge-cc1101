#pragma once
#include "freertos/idf_additions.h"
#include "mqtt_client.h"

#define MQTT_MESSAGE_QUEUE_LENGTH 4

typedef struct {
    char light_id;
    bool turn_on;
} mqtt_message_t;

esp_mqtt_client_handle_t mqtt_app_start(QueueHandle_t recv_queue);
