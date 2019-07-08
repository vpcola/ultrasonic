#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ultrasonic.h"
#include "esp_log.h"

#define ULTRASONICSENSOR_EVENT_LOOP_QUEUE_SIZE (16)
#define ULTRASONICSENSOR_PARSER_TASK_STACK_SIZE     CONFIG_ULTRASONICSENSOR_TASK_STACK_SIZE
#define ULTRASONICSENSOR_PARSER_TASK_PRIORITY       CONFIG_ULTRASONICSENSOR_TASK_PRIORITY

#define RMT_CLK_DIV 100 /* RMT counter clock divider */
#define RMT_TX_CARRIER_EN 0 /* Disable carrier */
#define rmt_item32_tIMEOUT_US 9500 /*!< RMT receiver timeout value(us) */
#define RMT_TICK_10_US (80000000/RMT_CLK_DIV/100000) /* RMT counter value for 10 us.(Source clock is APB clock) */
#define ITEM_DURATION(d) ((d & 0x7fff)*10/RMT_TICK_10_US)

ESP_EVENT_DEFINE_BASE(ESP_ULTRASONICSENSOR_EVENT);

static const char* ULTRASONIC_TAG = "UTSNC";

typedef struct {
    ultrasonicsensor_t parent;                     /*!< Parent class */
    rmt_channel_t   tx_channel;                    /*!< RMT tx (trigger) channel */
    rmt_channel_t   rx_channel;                    /*!< RMT rx (echo) channel */
    esp_event_loop_handle_t event_loop_hdl;        /*!< Event loop handle */
    TaskHandle_t tsk_hdl;                          /*!< Task handle */
    QueueHandle_t event_queue;                     /*!< Event queue handle */
} esp_ultrasonicsensor_t;

static void ultrasonicsensor_task_entry(void *arg)
{
    esp_ultrasonicsensor_t *esp_ultrasonicsensor = (esp_ultrasonicsensor_t *)arg;

     /* Create a 20us pulse for the TX channel */
    rmt_item32_t item;
    item.level0 = 1;
    item.duration0 = RMT_TICK_10_US;
    item.level1 = 0;
    item.duration1 = RMT_TICK_10_US; // for one pulse this doesn't matter

    /* Initialize the ring buffer for the RX channel */
    size_t rx_size = 0;
    RingbufHandle_t rb = NULL;
    rmt_get_ringbuf_handle(esp_ultrasonicsensor->rx_channel, &rb);
    rmt_rx_start(esp_ultrasonicsensor->rx_channel, 1);
   
    while (1) {
        /* Send the pulse to the TX channel */
        rmt_write_items(esp_ultrasonicsensor->tx_channel, &item, 1, true);
        rmt_wait_tx_done(esp_ultrasonicsensor->tx_channel, portMAX_DELAY);

        // Start receiving whatever data is received back
        rmt_item32_t* item = (rmt_item32_t*)xRingbufferReceive(rb, &rx_size, 1000);
        if(item){
            // distance = (high time * speed of sound (340.29 meters/sec)) /2;
            // convert the distance from meters to centimeters
            double distance = 340.29 * ITEM_DURATION(item->duration0) / (1000 * 1000 * 2); 
            ESP_LOGI(ULTRASONIC_TAG, "Distance is %.2f cm\n", distance * 100); // distance in centimeters
            vRingbufferReturnItem(rb, (void*) item);

            // Update the value!
            esp_ultrasonicsensor->parent.distance_cm = distance;
			// Post sensor update event 
            esp_event_post_to(esp_ultrasonicsensor->event_loop_hdl, ESP_ULTRASONICSENSOR_EVENT, ULTRASONICSENSOR_UPDATE,
            &(esp_ultrasonicsensor->parent), sizeof(ultrasonicsensor_t), 100 / portTICK_PERIOD_MS);

        } else {
            // There are time where no echo is received
            ESP_LOGI(ULTRASONIC_TAG, "Distance is not readable");
        }

        /* Drive the event loop */
        esp_event_loop_run(esp_ultrasonicsensor->event_loop_hdl, pdMS_TO_TICKS(50));
        /* Wait for the next reading */
        vTaskDelay(100 / portTICK_PERIOD_MS);
   }
    vTaskDelete(NULL);
}


ultrasonicsensor_handle_t ultrasonicsensor_init(const ultrasonicsensor_config_t * config)
{
    esp_ultrasonicsensor_t *esp_ultrasonicsensor = calloc(1, sizeof(esp_ultrasonicsensor_t));

    if (!esp_ultrasonicsensor) {
        ESP_LOGE(ULTRASONIC_TAG, "calloc memory for esp_ultrasonicsensor failed");
        goto err_ultrasonicsensor;
    }

    rmt_config_t rmt_tx={0};
    rmt_tx.channel = config->rmt.tx_channel;
    rmt_tx.gpio_num = config->rmt.trigger_pin;
    rmt_tx.mem_block_num = 1;
    rmt_tx.clk_div = RMT_CLK_DIV;
    rmt_tx.tx_config.loop_en = false;
    rmt_tx.tx_config.carrier_duty_percent = 50;
    rmt_tx.tx_config.carrier_freq_hz = 3000;
    rmt_tx.tx_config.carrier_level = 1;
    rmt_tx.tx_config.carrier_en = RMT_TX_CARRIER_EN;
    rmt_tx.tx_config.idle_level = 0;
    rmt_tx.tx_config.idle_output_en = true;
    rmt_tx.rmt_mode = 0;

    if (rmt_config(&rmt_tx) != ESP_OK)
    {
        ESP_LOGE(ULTRASONIC_TAG, "Call to rmt_config (tx) failed!!");
        goto err_tx_rmtconfig;
    }
    
    if (rmt_driver_install(rmt_tx.channel, 0, 0) != ESP_OK)
    {
        ESP_LOGE(ULTRASONIC_TAG, "Call to rmt_driver_install (tx) failed!");
        goto err_tx_driver_install;
    }

    rmt_config_t rmt_rx= {0};
    rmt_rx.channel = config->rmt.rx_channel;
    rmt_rx.gpio_num = config->rmt.echo_pin;
    rmt_rx.clk_div = RMT_CLK_DIV;
    rmt_rx.mem_block_num = 1;
    rmt_rx.rmt_mode = RMT_MODE_RX;
    rmt_rx.rx_config.filter_en = true;
    rmt_rx.rx_config.filter_ticks_thresh = 100;
    rmt_rx.rx_config.idle_threshold = rmt_item32_tIMEOUT_US / 10 * (RMT_TICK_10_US);

    if ( rmt_config(&rmt_rx) != ESP_OK)
    {
        ESP_LOGE(ULTRASONIC_TAG, "Call to rmt_config (rx) failed!!");
        goto err_rx_rmtconfig;
    }
    if ( rmt_driver_install(rmt_rx.channel, 1000, 0) != ESP_OK)
    {
        ESP_LOGE(ULTRASONIC_TAG, "Call to rmt_driver_install (rx) failed!");
        goto err_rx_driver_install;
    }

    // Sets the RX and TX RMT channels
    esp_ultrasonicsensor->tx_channel = config->rmt.tx_channel;
    esp_ultrasonicsensor->rx_channel = config->rmt.rx_channel;
    /* Create Event loop */
    esp_event_loop_args_t loop_args = {
        .queue_size = ULTRASONICSENSOR_EVENT_LOOP_QUEUE_SIZE,
        .task_name = NULL
    };
    if (esp_event_loop_create(&loop_args, &esp_ultrasonicsensor->event_loop_hdl) != ESP_OK) {
        ESP_LOGE(ULTRASONIC_TAG, "create event loop faild");
        goto err_eloop;
    }
    /* Create Ultrasonic sensor task */
    BaseType_t err = xTaskCreate(
            ultrasonicsensor_task_entry,
            "ultrasonicsensor",
            ULTRASONICSENSOR_PARSER_TASK_STACK_SIZE,
            esp_ultrasonicsensor,
            ULTRASONICSENSOR_PARSER_TASK_PRIORITY,
            &esp_ultrasonicsensor->tsk_hdl);

    if (err != pdTRUE) {
        ESP_LOGE(ULTRASONIC_TAG, "create dustsensor parser task failed");
        goto err_task_create;
    }

    ESP_LOGI(ULTRASONIC_TAG, "ULTRASONICSENSOR Parser init OK");
    return esp_ultrasonicsensor;

err_task_create:
    esp_event_loop_delete(esp_ultrasonicsensor->event_loop_hdl);
err_eloop:
err_rx_driver_install:
err_rx_rmtconfig:
    rmt_driver_uninstall(config->rmt.rx_channel);
err_tx_driver_install:
    rmt_driver_uninstall(config->rmt.tx_channel);
err_tx_rmtconfig:
err_ultrasonicsensor:
    free(esp_ultrasonicsensor); 

    return NULL;
}

esp_err_t ultrasonicsensor_deinit(ultrasonicsensor_handle_t ultrasonicsensor_hdl)
{
    esp_ultrasonicsensor_t *esp_ultrasonicsensor = (esp_ultrasonicsensor_t *) ultrasonicsensor_hdl;
    vTaskDelete(esp_ultrasonicsensor->tsk_hdl);
    esp_event_loop_delete(esp_ultrasonicsensor->event_loop_hdl);

    esp_err_t err = rmt_driver_uninstall(esp_ultrasonicsensor->rx_channel);
    err = rmt_driver_uninstall(esp_ultrasonicsensor->tx_channel);

    free(esp_ultrasonicsensor);
    return err;

}


esp_err_t ultrasonicsensor_add_handler(ultrasonicsensor_handle_t ultrasonicsensor_hdl, esp_event_handler_t event_handler, void *handler_args)
{
    esp_ultrasonicsensor_t *esp_ultrasonicsensor = (esp_ultrasonicsensor_t *)ultrasonicsensor_hdl;
    return esp_event_handler_register_with(esp_ultrasonicsensor->event_loop_hdl, ESP_ULTRASONICSENSOR_EVENT, ESP_EVENT_ANY_ID,
            event_handler, handler_args);
}

esp_err_t ultrasonicsensor_remove_handler(ultrasonicsensor_handle_t ultrasonicsensor_hdl, esp_event_handler_t event_handler)
{
    esp_ultrasonicsensor_t *esp_ultrasonicsensor = (esp_ultrasonicsensor_t *)ultrasonicsensor_hdl;
    return esp_event_handler_unregister_with(esp_ultrasonicsensor->event_loop_hdl, ESP_ULTRASONICSENSOR_EVENT, ESP_EVENT_ANY_ID, event_handler);
}


