#include <application.h>

// LED instance
bc_led_t led;

// Button instance
bc_button_t button;

bc_gfx_t *gfx;

bool sleep = false;

#define GRAPH_WIDTH_TIME (10 * 1000)
#define DISTANCE_MEASURE_PERIOD (200)

BC_DATA_STREAM_FLOAT_BUFFER(distance_stream_buffer, (GRAPH_WIDTH_TIME / DISTANCE_MEASURE_PERIOD))
bc_data_stream_t distance_stream;


void set_sleep()
{
    if (sleep)
    {
        bc_timer_start();
        bc_timer_delay(5000);
        bc_timer_stop();

        bc_gfx_clear(gfx);
        bc_gfx_set_font(gfx, &bc_font_ubuntu_33);
        bc_gfx_printf(gfx, 5, 10, true, "Sleep");
        bc_gfx_update(gfx);

        bc_timer_start();
        bc_timer_delay(5000);
        bc_timer_stop();

        bc_module_sensor_set_vdd(false);

        bc_scheduler_plan_absolute(0, BC_TICK_INFINITY);
    }
    else
    {
        bc_module_sensor_set_vdd(true);

        bc_scheduler_plan_now(0);
    }

}

void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param)
{
    if (event == BC_BUTTON_EVENT_PRESS)
    {
        bc_led_pulse(&led, 100);
        sleep = !sleep;

        set_sleep();
    }
}

void application_init(void)
{
    // Initialize logging
    bc_log_init(BC_LOG_LEVEL_DUMP, BC_LOG_TIMESTAMP_ABS);

    // Initialize LED
    bc_led_init(&led, BC_GPIO_LED, false, false);
    bc_led_set_mode(&led, BC_LED_MODE_ON);

    bc_module_lcd_init();
    gfx = bc_module_lcd_get_gfx();

    // Initialize button
    bc_button_init(&button, BC_GPIO_BUTTON, BC_GPIO_PULL_DOWN, false);
    bc_button_set_event_handler(&button, button_event_handler, NULL);

    bc_module_sensor_init();
    bc_module_sensor_set_vdd(true);

    bc_gpio_init(BC_GPIO_P4);
    bc_gpio_set_mode(BC_GPIO_P4, BC_GPIO_MODE_INPUT);

    bc_data_stream_init(&distance_stream, 1, &distance_stream_buffer);
}

void graph(bc_gfx_t *gfx, int x0, int y0, int x1, int y1, bc_data_stream_t *data_stream, int time_step, const char *format)
{
    int w, h;
    char str[32];
    int width = x1 - x0;
    int height = y1 - y0;
    float max_value = 0;
    float min_value = 0;

    bc_data_stream_get_max(data_stream, &max_value);

    bc_data_stream_get_min(data_stream, &min_value);

    if (min_value > 0)
    {
        min_value = 0;
    }

    max_value = ceilf(max_value / 5) * 5;

    bc_module_lcd_set_font(&bc_font_ubuntu_11);

    int number_of_samples = bc_data_stream_get_number_of_samples(data_stream);

    int end_time = - number_of_samples * time_step / 1000;

    h = 10;

    float range = fabsf(max_value) + fabsf(min_value);
    float fh = height - h - 2;

    snprintf(str, sizeof(str), "%ds", end_time);
    w = bc_gfx_calc_string_width(gfx, str) + 8;

    int lines = width / w;
    int y_time = y1 - h - 2;
    int y_zero = range > 0 ? y_time - ((fabsf(min_value) / range) * fh) : y_time;
    int tmp;

    for (int i = 0, time_step = end_time / lines, w_step = width / lines; i < lines; i++)
    {
        snprintf(str, sizeof(str), "%ds", time_step * i);

        w = bc_gfx_calc_string_width(gfx, str);

        tmp = width - w_step * i;

        bc_gfx_draw_string(gfx, tmp - w, y1 - h, str, 1);

        bc_gfx_draw_line(gfx, tmp - 2, y_zero - 2, tmp - 2, y_zero + 2, 1);

        bc_gfx_draw_line(gfx, tmp - 2, y0, tmp - 2, y0 + 2, 1);

        if (y_time != y_zero)
        {
            bc_gfx_draw_line(gfx, tmp - 2, y_time - 2, tmp - 2, y_time, 1);
        }
    }

    bc_gfx_draw_line(gfx, x0, y_zero, x1, y_zero, 1);

    if (y_time != y_zero)
    {
        bc_gfx_draw_line(gfx, x0, y_time, y1, y_time, 1);

        snprintf(str, sizeof(str), format, min_value);

        bc_gfx_draw_string(gfx, x0, y_time - 10, str, 1);
    }

    bc_gfx_draw_line(gfx, x0, y0, x1, y0, 1);

    snprintf(str, sizeof(str), format, max_value);

    bc_gfx_draw_string(gfx, x0, y0, str, 1);

    bc_gfx_draw_string(gfx, x0, y_zero - 10, "0", 1);

    if (range == 0)
    {
        return;
    }

    int length = bc_data_stream_get_length(data_stream);
    float value;

    int x_zero = x1 - 2;
    float fy;

    int dx = width / (number_of_samples - 1);
    int point_x = x_zero + dx;
    int point_y;
    int last_x;
    int last_y;

    min_value = fabsf(min_value);

    for (int i = 1; i <= length; i++)
    {
        if (bc_data_stream_get_nth(data_stream, -i, &value))
        {
            fy = (value + min_value) / range;

            point_y = y_time - (fy * fh);
            point_x -= dx;

            if (i == 1)
            {
                last_y = point_y;
                last_x = point_x;
            }

            bc_gfx_draw_line(gfx, point_x, point_y, last_x, last_y, 1);

            last_y = point_y;
            last_x = point_x;

        }
    }
}


void application_task(void)
{
    bc_system_pll_enable();

    // Wait while signal is low
    while (bc_gpio_get_input(BC_GPIO_P4));

    // Wait until signal is high - rising edge
    while (!bc_gpio_get_input(BC_GPIO_P4));

    bc_timer_start();

    // Wait while signal is low
    while (bc_gpio_get_input(BC_GPIO_P4));

    uint32_t microseconds = bc_timer_get_microseconds();

    bc_timer_stop();

    bc_gfx_clear(gfx);
    float centimeters = microseconds / 58.0f;

    bc_gfx_set_font(gfx, &bc_font_ubuntu_33);
    bc_gfx_printf(gfx, 5, 10, true, "%.1f", centimeters);

    bc_gfx_set_font(gfx, &bc_font_ubuntu_24);
    bc_gfx_printf(gfx, 90, 15, true, "cm");

    bc_data_stream_feed(&distance_stream, &centimeters);
    graph(gfx, 0, 40, 127, 127, &distance_stream, DISTANCE_MEASURE_PERIOD, "%.1f");


    bc_gfx_update(gfx);

    bc_log_debug("%d cm", centimeters);

    bc_system_pll_disable();

    bc_scheduler_plan_current_from_now(DISTANCE_MEASURE_PERIOD);
}
