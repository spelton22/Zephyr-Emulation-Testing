#ifndef BUTTON_TEST_SUPPORT_H
#define BUTTON_TEST_SUPPORT_H

#include <zephyr/kernel.h>

extern struct k_thread student_main_thread;
extern k_tid_t student_main_tid;
extern volatile bool main_is_running;

void student_main_thread_entry(void *p1, void *p2, void *p3);
void test_before(void *fixture);
void test_after(void *fixture);

extern struct k_event button_events;
extern int LED_STATE;

#define LED_ON 1
#define LED_OFF 0

#define BUTTON_EVENT BIT(0)

extern k_thread_stack_t student_main_stack;
extern struct k_thread student_main_thread;
extern k_tid_t student_main_tid;
extern volatile bool main_is_running;

#endif