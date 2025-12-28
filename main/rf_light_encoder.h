#pragma once

#include <stdint.h>
#include "driver/rmt_encoder.h"
#include "driver/rmt_types.h"
#include "esp_err.h"
#include "hal/rmt_types.h"

typedef uint16_t rf_light_message_t;
#define RF_LIGHT_PAYLOAD_ZERO_DURATION_0  263
#define RF_LIGHT_PAYLOAD_ZERO_DURATION_1  (843-263)
#define RF_LIGHT_PAYLOAD_ONE_DURATION_0   685
#define RF_LIGHT_PAYLOAD_ONE_DURATION_1   (843-685)

/// Defines what has _already been sent_
typedef enum {
    RF_LIGHT_ENCODER_STATE_RESET,
    RF_LIGHT_ENCODER_STATE_HEADER_DONE,
    RF_LIGHT_ENCODER_STATE_PAYLOAD_0_DONE,
    RF_LIGHT_ENCODER_STATE_DELAY_0_DONE,
    RF_LIGHT_ENCODER_STATE_PAYLOAD_1_DONE,
    RF_LIGHT_ENCODER_STATE_DELAY_1_DONE,
    RF_LIGHT_ENCODER_STATE_PAYLOAD_2_DONE,
    RF_LIGHT_ENCODER_STATE_DELAY_2_DONE,
    RF_LIGHT_ENCODER_STATE_PAYLOAD_3_DONE
} rf_light_encoder_state_t;

typedef struct {
    rmt_encoder_t base;
    // bytes encoder
    rmt_encoder_t *header_encoder;
    // bytes encoder
    rmt_encoder_t *payload_encoder;
    rmt_encoder_t *copy_encoder;
    rmt_symbol_word_t delay_symbol;
    rf_light_encoder_state_t state;
    uint8_t payload_num;
} rf_light_encoder_t;

esp_err_t rf_light_encoder_new(rmt_encoder_handle_t *encoder);
