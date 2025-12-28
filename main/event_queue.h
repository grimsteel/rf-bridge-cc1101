#pragma once
#include "mqtt.h"
#include "rf_light_encoder.h"

typedef union {
    mqtt_message_t mqtt_message;
    rf_light_message_t rf_light_message;
} event_queue_message_data_t;

typedef enum {
    EVENT_QUEUE_MESSAGE_MQTT,
    EVENT_QUEUE_MESSAGE_RF_LIGHT,
} event_queue_message_type_t;

typedef struct {
    event_queue_message_type_t type;
    event_queue_message_data_t data;
} event_queue_message_t;
