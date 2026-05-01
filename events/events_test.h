#ifndef LED_TEST_H
#define LED_TEST_H

/* ------------------------------------------------------------------ */
/*  Student GPIOs                                                     */
/* ------------------------------------------------------------------ */
extern const struct gpio_dt_spec button;
extern const struct gpio_dt_spec blinking_led;

extern struct k_event button_events;

/* Event bits */
#define EVENT_LED_ON      BIT(0)
#define EVENT_LED_OFF     BIT(1)
#define EVENT_BUTTON      BIT(2)

#endif // LED_TEST_H