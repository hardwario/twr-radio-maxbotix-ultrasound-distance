#include <application.h>

// LED instance
twr_led_t led;

// Button instance
twr_button_t button;

#define MEASUREMENT_REPORT_INTERVAL (15 * 60 * 1000)

void button_event_handler(twr_button_t *self, twr_button_event_t event, void *event_param)
{
    if (event == TWR_BUTTON_EVENT_PRESS)
    {
        twr_led_pulse(&led, 100);
    }
}

void application_init(void)
{
    // Initialize logging
    twr_log_init(TWR_LOG_LEVEL_DUMP, TWR_LOG_TIMESTAMP_ABS);

    // Initialize LED
    twr_led_init(&led, TWR_GPIO_LED, false, false);
    twr_led_pulse(&led, 2000);

    // Initialize button
    twr_button_init(&button, TWR_GPIO_BUTTON, TWR_GPIO_PULL_DOWN, false);
    twr_button_set_event_handler(&button, button_event_handler, NULL);

    twr_module_sensor_init();

    twr_gpio_init(TWR_GPIO_P4);
    twr_gpio_set_mode(TWR_GPIO_P4, TWR_GPIO_MODE_INPUT);

    twr_radio_init(TWR_RADIO_MODE_NODE_SLEEPING);
    twr_radio_pairing_request("ultrasound-distance", VERSION);
}

void application_task(void)
{
    twr_module_sensor_set_vdd(true);
    twr_system_pll_enable();
    twr_tick_wait(300);

    twr_tick_t timeout = twr_tick_get() + 5000;

    // Wait while signal is low
    while (twr_gpio_get_input(TWR_GPIO_P4))
    {
        if (twr_tick_get() >= timeout)
        {
            timeout = 0;
            break;
        }
    }

    // Wait until signal is high - rising edge
    while (!twr_gpio_get_input(TWR_GPIO_P4))
    {
        if (twr_tick_get() >= timeout)
        {
            timeout = 0;
            break;
        }
    }

    twr_timer_start();

    // Wait while signal is low
    while (twr_gpio_get_input(TWR_GPIO_P4))
    {
        if (twr_tick_get() >= timeout)
        {
            timeout = 0;
            break;
        }
    }

    uint32_t microseconds = twr_timer_get_microseconds();

    twr_timer_stop();

    float centimeters = microseconds / 58.0f;

    twr_system_pll_disable();
    twr_module_sensor_set_vdd(false);

    twr_scheduler_plan_current_from_now(MEASUREMENT_REPORT_INTERVAL);

    if (timeout == 0)
    {
        twr_log_error("APP: Sensor error");
        twr_radio_pub_float("distance/-/centimeters", NULL);
    }
    else
    {
        twr_log_info("APP: Distance = %.1f cm", centimeters);
        twr_radio_pub_float("distance/-/centimeters", &centimeters);
    }
}
