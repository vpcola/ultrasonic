// Copyright 2015-2018 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/rmt.h"
#include "esp_types.h"
#include "esp_event.h"
#include "esp_err.h"
#include "driver/periph_ctrl.h"
#include "driver/gpio.h"

ESP_EVENT_DECLARE_BASE(ESP_ULTRASONICSENSOR_EVENT);

typedef struct {
    double distance_cm;
} ultrasonicsensor_t;

typedef struct {
 struct {
     rmt_channel_t  tx_channel;   /*!< RMT TX Channel to use */
     gpio_num_t     trigger_pin;       /*!< GPIO pin for TX */
     rmt_channel_t  rx_channel;   /*!< RMT RX Channel to use */
     gpio_num_t     echo_pin;       /*!< GPIO pin for RX */
 } rmt;                           /*!< RMT specific configuration */
 uint32_t read_interval;
} ultrasonicsensor_config_t;

typedef void * ultrasonicsensor_handle_t;

#define ULTRASONICSENSOR_CONFIG_DEFAULT()       \
{                                      \
    .rmt = {                           \
        .tx_channel = RMT_CHANNEL_1,   \
        .trigger_pin = GPIO_NUM_18,    \
        .rx_channel = RMT_CHANNEL_0,   \
        .echo_pin = GPIO_NUM_19,       \
    },                                 \
    .read_interval = 100               \
}

typedef enum {
    ULTRASONICSENSOR_UPDATE, 
    ULTRASONICSENSOR_UNKNOWN 
} ultrasonicsensor_event_id_t;

ultrasonicsensor_handle_t ultrasonicsensor_init(const ultrasonicsensor_config_t * config);
esp_err_t ultrasonicsensor_deinit(ultrasonicsensor_handle_t ultrasonicsensor_hdl);

esp_err_t ultrasonicsensor_add_handler(ultrasonicsensor_handle_t ultrasonicsensor_hdl, esp_event_handler_t event_handler, void *handler_args);
esp_err_t ultrasonicsensor_remove_handler(ultrasonicsensor_handle_t ultrasonicsensor_hdl, esp_event_handler_t event_handler);


esp_err_t ultrasonic_getdistance(double * distance);

#ifdef __cplusplus
}
#endif
