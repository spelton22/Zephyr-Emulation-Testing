#ifndef ZEPHYR_BUTTON_TEST_H
#define ZEPHYR_BUTTON_TEST_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define STUDENT_MAIN_STACK_SIZE 2048
#define STUDENT_MAIN_PRIORITY   5

extern int student_main(void);  /* renamed by CMake */

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

void simulate_button_click(const struct gpio_dt_spec *button);






// extern struct k_thread student_main_thread;
// extern k_tid_t student_main_tid;
// extern volatile bool main_is_running;

// void student_main_thread_entry(void *p1, void *p2, void *p3);
// void test_before(void *fixture);
// void test_after(void *fixture);

// extern struct k_event button_events;
// extern int LED_STATE;

// #define LED_ON 1
// #define LED_OFF 0

// #define BUTTON_EVENT BIT(0)

// extern k_thread_stack_t student_main_stack;
// extern struct k_thread student_main_thread;
// extern k_tid_t student_main_tid;
// extern volatile bool main_is_running;

#endif // ZEPHYR_BUTTON_TEST_H