#pragma once

#include "cc1101.h"

esp_err_t init_cc1101(cc1101_device_t** cc1101_handle);
esp_err_t cc1101_start_rx(cc1101_device_t* cc1101_handle);
esp_err_t cc1101_start_tx(cc1101_device_t* cc1101_handle);
