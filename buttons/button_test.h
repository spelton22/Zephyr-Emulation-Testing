#ifndef BUTTON_TEST_H
#define BUTTON_TEST_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "zephyr_button_test_lib.h"

extern struct k_event button_events;
extern int LED_STATE;

#define LED_ON 1
#define LED_OFF 0

#define BUTTON_EVENT1 BIT(0)
#define BUTTON_EVENT2 BIT(1)

extern const struct gpio_dt_spec button_test;

// extern void button_test_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins);

#endif // BUTTON_TEST_H
