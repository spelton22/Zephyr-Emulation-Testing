#ifndef ZEPHYR_BUTTON_TEST_LIB_H
#define ZEPHYR_BUTTON_TEST_LIB_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

/* ---- student_main symbols (defined in main.c) ---- */
extern int student_main(void);
extern struct k_event button_events;
extern int LED_STATE;
extern const struct gpio_dt_spec button_test;

/* ---- event/LED constants ---- */
#define LED_ON        1
#define LED_OFF       0
#define BUTTON_EVENT1 BIT(0)

/* ---- thread config ---- */
#define STUDENT_MAIN_STACK_SIZE 2048
#define STUDENT_MAIN_PRIORITY   5

extern struct k_thread student_main_thread;
extern k_tid_t         student_main_tid;
extern volatile bool   main_running;

/* ---- test helpers ---- */
void before(void *);
void after(void *);
void student_main_entry(void *, void *, void *);
void stop_main(void);
void start_main(int settle_ms);
void simulate_button_click(const struct gpio_dt_spec *button);
bool wait_for_event(uint32_t mask, int timeout_ms);

#endif // ZEPHYR_BUTTON_TEST_LIB_H