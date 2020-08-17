#include <application.h>

// LED instance
bc_led_t led;

// Button instance
bc_button_t button;

#define MEASUREMENT_REPORT_INTERVAL (15 * 60 * 1000)

void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param)
{
    if (event == BC_BUTTON_EVENT_PRESS)
    {
        bc_led_pulse(&led, 100);
    }
}

void application_init(void)
{
    // Initialize logging
    bc_log_init(BC_LOG_LEVEL_DUMP, BC_LOG_TIMESTAMP_ABS);

    // Initialize LED
    bc_led_init(&led, BC_GPIO_LED, false, false);
    bc_led_pulse(&led, 2000);

    // Initialize button
    bc_button_init(&button, BC_GPIO_BUTTON, BC_GPIO_PULL_DOWN, false);
    bc_button_set_event_handler(&button, button_event_handler, NULL);

    bc_module_sensor_init();

    bc_gpio_init(BC_GPIO_P4);
    bc_gpio_set_mode(BC_GPIO_P4, BC_GPIO_MODE_INPUT);

    bc_radio_init(BC_RADIO_MODE_NODE_SLEEPING);
    bc_radio_pairing_request("ultrasound-distance", VERSION);
}

void application_task(void)
{
    bc_module_sensor_set_vdd(true);
    bc_system_pll_enable();
    bc_tick_wait(300);

    bc_tick_t timeout = bc_tick_get() + 5000;

    // Wait while signal is low
    while (bc_gpio_get_input(BC_GPIO_P4))
    {
        if (bc_tick_get() >= timeout)
        {
            timeout = 0;
            break;
        }
    }

    // Wait until signal is high - rising edge
    while (!bc_gpio_get_input(BC_GPIO_P4))
    {
        if (bc_tick_get() >= timeout)
        {
            timeout = 0;
            break;
        }
    }

    bc_timer_start();

    // Wait while signal is low
    while (bc_gpio_get_input(BC_GPIO_P4))
    {
        if (bc_tick_get() >= timeout)
        {
            timeout = 0;
            break;
        }
    }

    uint32_t microseconds = bc_timer_get_microseconds();

    bc_timer_stop();

    float centimeters = microseconds / 58.0f;

    bc_system_pll_disable();
    bc_module_sensor_set_vdd(false);

    bc_scheduler_plan_current_from_now(MEASUREMENT_REPORT_INTERVAL);

    if (timeout == 0)
    {
        bc_log_error("APP: Sensor error");
        bc_radio_pub_float("distance/-/centimeters", NULL);
    }
    else
    {
        bc_log_info("APP: Distance = %.1f cm", centimeters);
        bc_radio_pub_float("distance/-/centimeters", &centimeters);
    }
}
