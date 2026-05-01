#ifndef BUTTON_TEST_SUPPORT_H
#define BUTTON_TEST_SUPPORT_H

#include <zephyr/kernel.h>

extern struct k_thread student_main_thread;
extern k_tid_t student_main_tid;
extern volatile bool main_is_running;

void student_main_thread_entry(void *p1, void *p2, void *p3);
void test_before(void *fixture);
void test_after(void *fixture);

#endif