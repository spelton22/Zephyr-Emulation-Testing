#ifndef ZEPHYR_EVENTS_TEST_H
#define ZEPHYR_EVENTS_TEST_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define LED_FREQ 1

/* ------------------------------------------------------------------ */
/*  Thread config for running student_main in background              */
/* ------------------------------------------------------------------ */
#define STUDENT_MAIN_STACK_SIZE 2048
#define STUDENT_MAIN_PRIORITY   5

extern int student_main(void);  /* renamed by CMake */

/* ------------------------------------------------------------------ */
/*  OUR FUCTIONS                                                      */
/* ------------------------------------------------------------------ */
// fixture
void before(void *);
void after(void *);

// thread boilerplate
extern struct k_thread student_main_thread;
extern k_tid_t student_main_tid;
extern volatile bool main_running;
void student_main_entry(void *, void *, void *);
void stop_main(void);
void start_main(int settle_ms);

// helpers
void assert_led_blink_freq(const struct gpio_dt_spec *led,
                            int window_ms,
                            int expected_hz,
                            int tolerance_hz,
                            const char *led_name);
void simulate_button_click(const struct gpio_dt_spec *button);
void assert_led_off(const struct gpio_dt_spec *led, const char *led_name);
void assert_led_on(const struct gpio_dt_spec *led, const char *led_name);
bool is_led_on(const struct gpio_dt_spec *led);
void assert_led_duty_cycle(const struct gpio_dt_spec *led,
                            const char *name,
                            int window_ms,
                            int expected_duty,
                            int tolerance);

struct duty_ctx {
    const struct gpio_dt_spec *led;

    int64_t last_ts;
    bool last_state;

    int64_t on_time;
    int64_t total_time;
};

#endif // ZEPHYR_EVENTS_TEST_H