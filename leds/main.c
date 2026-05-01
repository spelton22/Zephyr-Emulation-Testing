/**
 * This is Zephyr's blinky sample project, used to showcase how the ztest
 * helper functions can be used.
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   500

static const struct gpio_dt_spec blinker = GPIO_DT_SPEC_GET(DT_ALIAS(blinking), gpios);

int main(void)
{
    int ret;
    bool led_state = true;

    if (!gpio_is_ready_dt(&blinker)) {
        return 0;
    }

    ret = gpio_pin_configure_dt(&blinker, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        return 0;
    }

    while (1) {
        ret = gpio_pin_toggle_dt(&blinker);
        if (ret < 0) {
            return 0;
        }

        led_state = !led_state;
        printf("LED state: %s\n", led_state ? "ON" : "OFF");
        k_msleep(SLEEP_TIME_MS);
    }
    
    return 0;
}
