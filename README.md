# Ultrasonic sensor ESP-IDF component

This is a sample driver for HC-SR04 and variants (with trigger and echo pins).

# Using this component with ESP-IDF

Create a components directory and clone this component using

    >cd components
    >git submodule add https://github.com/vpcola/ultrasonic.git

# Wiring

Use any GPIO pins and assign them using the following code:

    /* Dustsensor parser configuration */
    ultrasonicsensor_config_t config = ULTRASONICSENSOR_CONFIG_DEFAULT();
    config.read_interval = 1000;
    config.rmt.trigger_pin = GPIO_NUM_15;
    config.rmt.echo_pin = GPIO_NUM_4;

# Callback functions

Updates from the sensor comes via callback functions. After initializing the component, you must register an event handler via
ultrasonicsensor_add_handler() to register the callback function. Once registered, you can then listen
for UTRASONICSENSOR_UPDATE event. You can specify the update interval (how often do we query the ultrasonic
sensor) of the ultrasonic sensor before initialization.

Here's a sample code on the use of this component.

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


