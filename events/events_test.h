#ifndef EVENTS_TEST_H
#define EVENTS_TEST_H

/* ------------------------------------------------------------------ */
/*  Student GPIOs                                                     */
/* ------------------------------------------------------------------ */
extern const struct gpio_dt_spec button;
extern const struct gpio_dt_spec blinking_led;

extern struct k_event program_events;

/* Event bits */
#define EVENT_LED_ON      BIT(0)
#define EVENT_LED_OFF     BIT(1)
#define EVENT_BUTTON      BIT(2)

#endif // EVENTS_TEST_H