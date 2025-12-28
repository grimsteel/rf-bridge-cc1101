#include "rf_light_encoder.h"
#include "driver/rmt_encoder.h"
#include "driver/rmt_types.h"
#include "esp_check.h"
#include "esp_err.h"
#include "hal/rmt_types.h"

#define TAG "rf-light-encoder"

#ifndef RMT_ENCODER_FUNC_ATTR
#define RMT_ENCODER_FUNC_ATTR
#endif

// 5 bytes = 40 bits, last bit 0 so w/ delay
static uint64_t RF_LIGHT_HEADER = 0x7fffffffff;
#define RF_LIGHT_NUM_PAYLOAD 4

RMT_ENCODER_FUNC_ATTR
static inline void encode_payload(rmt_channel_handle_t channel, rf_light_message_t *message, rf_light_encoder_t *rf_light_encoder, rmt_encode_state_t *state, size_t* encoded_symbols) {
    rmt_encode_state_t session_state;
    *encoded_symbols += rf_light_encoder->payload_encoder->encode(rf_light_encoder->payload_encoder, channel, message, sizeof(rf_light_message_t), &session_state);
    if (session_state & RMT_ENCODING_COMPLETE) {
        // advance
        rf_light_encoder->state++;
    }
    // quit for now if no space
    if (session_state & RMT_ENCODING_MEM_FULL) {
        *state |= RMT_ENCODING_MEM_FULL;
    }
}

RMT_ENCODER_FUNC_ATTR
static inline void encode_delay(rmt_channel_handle_t channel, rf_light_encoder_t *rf_light_encoder, rmt_encode_state_t *state, size_t* encoded_symbols) {
    rmt_encode_state_t session_state;
    *encoded_symbols += rf_light_encoder->copy_encoder->encode(rf_light_encoder->copy_encoder, channel, &rf_light_encoder->delay_symbol, sizeof(rmt_symbol_word_t), &session_state);
    if (session_state & RMT_ENCODING_COMPLETE) {
        // advance
        rf_light_encoder->state++;
    }
    // quit for now if no space
    if (session_state & RMT_ENCODING_MEM_FULL) {
        *state |= RMT_ENCODING_MEM_FULL;
    }
}

RMT_ENCODER_FUNC_ATTR
static size_t rf_light_encode(rmt_encoder_t* encoder, rmt_channel_handle_t channel, const void* data, size_t data_len, rmt_encode_state_t* ret_state) {
    rf_light_encoder_t* rf_light_encoder = __containerof(encoder, rf_light_encoder_t, base);
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    rmt_encode_state_t state = RMT_ENCODING_RESET;
    size_t encoded_symbols = 0;
    rf_light_message_t *message = (rf_light_message_t*) data;

    switch (rf_light_encoder->state) {
    case RF_LIGHT_ENCODER_STATE_RESET:
        // 5 bytes
        encoded_symbols += rf_light_encoder->header_encoder->encode(rf_light_encoder->header_encoder, channel, &RF_LIGHT_HEADER, 5, &session_state);
        // advance if complete
        if (session_state & RMT_ENCODING_COMPLETE) rf_light_encoder->state++;
        // quit for now if no space
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;
            break;
        }
    // fall-through
    case RF_LIGHT_ENCODER_STATE_HEADER_DONE:
        // encode first payload
        encode_payload(channel, message, rf_light_encoder, &state, &encoded_symbols);
        // quit for now if no space
        if (state & RMT_ENCODING_MEM_FULL) break;
    // fall-through
    case RF_LIGHT_ENCODER_STATE_PAYLOAD_0_DONE:
        // encode delay
        encode_delay(channel, rf_light_encoder, &state, &encoded_symbols);
        // quit for now if no space
        if (state & RMT_ENCODING_MEM_FULL) break;
    // fall-through
    case RF_LIGHT_ENCODER_STATE_DELAY_0_DONE:
        encode_payload(channel, message, rf_light_encoder, &state, &encoded_symbols);
        if (state & RMT_ENCODING_MEM_FULL) break;
    // fall-through
    case RF_LIGHT_ENCODER_STATE_PAYLOAD_1_DONE:
        encode_delay(channel, rf_light_encoder, &state, &encoded_symbols);
        if (state & RMT_ENCODING_MEM_FULL) break;
    // fall-through
    case RF_LIGHT_ENCODER_STATE_DELAY_1_DONE:
        encode_payload(channel, message, rf_light_encoder, &state, &encoded_symbols);
        if (state & RMT_ENCODING_MEM_FULL) break;
    // fall-through
    case RF_LIGHT_ENCODER_STATE_PAYLOAD_2_DONE:
        encode_delay(channel, rf_light_encoder, &state, &encoded_symbols);
        if (state & RMT_ENCODING_MEM_FULL) break;
    // fall-through
    case RF_LIGHT_ENCODER_STATE_DELAY_2_DONE:
        encode_payload(channel, message, rf_light_encoder, &state, &encoded_symbols);
        if (state & RMT_ENCODING_MEM_FULL) break;
    // fall-through
    case RF_LIGHT_ENCODER_STATE_PAYLOAD_3_DONE:
        state |= RMT_ENCODING_COMPLETE;
        rf_light_encoder->state = RF_LIGHT_ENCODER_STATE_RESET;
    }
    *ret_state = state;
    return encoded_symbols;
}

static esp_err_t rf_light_del_encoder(rmt_encoder_t* encoder) {
    rf_light_encoder_t* rf_light_encoder = __containerof(encoder, rf_light_encoder_t, base);
    rmt_del_encoder(rf_light_encoder->header_encoder);
    rmt_del_encoder(rf_light_encoder->payload_encoder);
    rmt_del_encoder(rf_light_encoder->copy_encoder);
    free(rf_light_encoder);
    return ESP_OK;
}

RMT_ENCODER_FUNC_ATTR
static esp_err_t rf_light_reset_encoder(rmt_encoder_t* encoder) {
    rf_light_encoder_t* rf_light_encoder = __containerof(encoder, rf_light_encoder_t, base);
    rmt_encoder_reset(rf_light_encoder->header_encoder);
    rmt_encoder_reset(rf_light_encoder->payload_encoder);
    rmt_encoder_reset(rf_light_encoder->copy_encoder);
    rf_light_encoder->state = RF_LIGHT_ENCODER_STATE_RESET;
    return ESP_OK;
}

esp_err_t rf_light_encoder_new(rmt_encoder_handle_t *encoder) {
    // needed for GOTO_ON_FALSE
    esp_err_t ret = ESP_OK;
    rf_light_encoder_t *rf_light_encoder = NULL;
    ESP_GOTO_ON_FALSE(encoder, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    rf_light_encoder = rmt_alloc_encoder_mem(sizeof(rf_light_encoder_t));
    ESP_GOTO_ON_FALSE(rf_light_encoder, ESP_ERR_NO_MEM, err, TAG, "no mem for encoder");
    rf_light_encoder->base.encode = rf_light_encode;
    rf_light_encoder->base.del = rf_light_del_encoder;
    rf_light_encoder->base.reset = rf_light_reset_encoder;
    *encoder = &rf_light_encoder->base;

    rmt_bytes_encoder_config_t header_encoder_config = {
        // 1 w/ delay
        .bit0 = {
            .level0 = 1,
            .duration0 = 264 / 2, // 264 us, 1 tick = 2 us
            .level1 = 0,
            .duration1 = (4000 + 160) / 2
        },
        // identical
        .bit1 = {
            .level0 = 1,
            .duration0 = 264 / 2, // 264 us, 1 tick = 2 us
            .level1 = 0,
            .duration1 = 160 / 2
        },
        .flags.msb_first = false
    };
    ESP_GOTO_ON_ERROR(rmt_new_bytes_encoder(&header_encoder_config, &rf_light_encoder->header_encoder), err, TAG, "create header encoder failed");

    rmt_bytes_encoder_config_t payload_encoder_config = {
        .bit0 = {
            .level0 = 1,
            .duration0 = RF_LIGHT_PAYLOAD_ZERO_DURATION_0 / 2, // 264 us, 1 tick = 2 us
            .level1 = 0,
            .duration1 = RF_LIGHT_PAYLOAD_ZERO_DURATION_1 / 2
        },
        // identical
        .bit1 = {
            .level0 = 1,
            .duration0 = RF_LIGHT_PAYLOAD_ONE_DURATION_0 / 2, // 264 us, 1 tick = 2 us
            .level1 = 0,
            .duration1 = RF_LIGHT_PAYLOAD_ZERO_DURATION_1 / 2
        },
    };
    ESP_GOTO_ON_ERROR(rmt_new_bytes_encoder(&payload_encoder_config, &rf_light_encoder->payload_encoder), err, TAG, "create payload encoder failed");

    // init copy encoder and symbol
    rmt_copy_encoder_config_t copy_encoder_config = {};
    ESP_GOTO_ON_ERROR(rmt_new_copy_encoder(&copy_encoder_config, &rf_light_encoder->copy_encoder), err, TAG, "create copy encoder failed");

    rf_light_encoder->delay_symbol.duration0 = 2000 / 2;
    rf_light_encoder->delay_symbol.duration1 = 2000 / 2; // 4 ms total
    rf_light_encoder->delay_symbol.level0 = 0;
    rf_light_encoder->delay_symbol.level1 = 0;

    return ESP_OK;

    err:
    // clean up
    if (rf_light_encoder) {
        if (rf_light_encoder->header_encoder) rmt_del_encoder(rf_light_encoder->header_encoder);
        if (rf_light_encoder->payload_encoder) rmt_del_encoder(rf_light_encoder->payload_encoder);
        if (rf_light_encoder->copy_encoder) rmt_del_encoder(rf_light_encoder->copy_encoder);
        free(rf_light_encoder);
    }
    return ret;
}
