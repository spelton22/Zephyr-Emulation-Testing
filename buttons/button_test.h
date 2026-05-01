#ifndef TEST_BUTTON_CALLBACK_H
#define TEST_BUTTON_CALLBACK_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

extern struct k_event button_events;
extern int LED_STATE;

#define LED_ON 1
#define LED_OFF 0

#define BUTTON_EVENT BIT(0)

extern void button_test_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins);

#endif 
