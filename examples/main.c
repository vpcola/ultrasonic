/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "ultrasonic.h"


static void ultrasonicsensor_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ultrasonicsensor_t *sensor = NULL;
    switch (event_id) {
    case ULTRASONICSENSOR_UPDATE:
        sensor = (ultrasonicsensor_t *)event_data;
        // Handle the data from the sensor here
        printf("Distance : %.2f cm\n", sensor->distance_cm);
        break;
    case ULTRASONICSENSOR_UNKNOWN:
        /* print unknown statements */
        printf("Unknown statement:%s", (char *)event_data);
        break;
    default:
        break;
    }
}

void app_main()
{
    /* Dustsensor parser configuration */
    ultrasonicsensor_config_t config = ULTRASONICSENSOR_CONFIG_DEFAULT();
    config.read_interval = 1000;
    config.rmt.trigger_pin = GPIO_NUM_15;
    config.rmt.echo_pin = GPIO_NUM_4;
    /* init Dustsensor parser library */
    ultrasonicsensor_handle_t ultrasonicsensor_hdl = ultrasonicsensor_init(&config);
    /* register event handler for Dustsensor parser library */
    ultrasonicsensor_add_handler(ultrasonicsensor_hdl, ultrasonicsensor_event_handler, NULL);


    while(1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

        /* unregister event handler */
    ultrasonicsensor_remove_handler(ultrasonicsensor_hdl, ultrasonicsensor_event_handler);
    /* deinit Dustsensor parser library */
    ultrasonicsensor_deinit(ultrasonicsensor_hdl);
}

