#ifndef SMF_TEST_H
#define SMF_TEST_H

/* ------------------------------------------------------------------ */
/*  Student GPIOs                                                     */
/* ------------------------------------------------------------------ */
extern const struct gpio_dt_spec led_left;
extern const struct gpio_dt_spec led_right;
extern const struct gpio_dt_spec btn_left;
extern const struct gpio_dt_spec btn_right;
extern const struct gpio_dt_spec btn_stop;

extern struct k_event program_events;

extern struct app_ctx s_ctx;
enum state_ids {
    INIT,
    IDLE,
    BLINK_LEFT,
    BLINK_RIGHT,
};

/* Input events */
#define EVT_SELECT_LEFT   BIT(0)
#define EVT_SELECT_RIGHT  BIT(1)
#define EVT_STOP          BIT(2)

/* State entry events */
#define EVT_ENTER_IDLE        BIT(8)
#define EVT_ENTER_BLINK_LEFT  BIT(9)
#define EVT_ENTER_BLINK_RIGHT BIT(10)

#endif // SMF_TEST_H