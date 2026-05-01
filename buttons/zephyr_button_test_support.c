#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include "zephyr_button_test_support.h"

/* Thread for running student's main code */
#define STUDENT_MAIN_STACK_SIZE 1024
#define STUDENT_MAIN_PRIORITY 5

// K_THREAD_STACK_DEFINE(student_main_stack, STUDENT_MAIN_STACK_SIZE);
// struct k_thread student_main_thread;
// k_tid_t student_main_tid;

// volatile bool main_is_running = false;

extern int student_main(void);

/**
 * @brief Wrapper to run student_main in a thread
 */
void student_main_thread_entry(void *p1, void *p2, void *p3)
{
  ARG_UNUSED(p1);
  ARG_UNUSED(p2);
  ARG_UNUSED(p3);

  main_is_running = true;
  student_main();
  main_is_running = false;
}

/**
 * @brief Test fixture setup
 */
void test_before(void *fixture)
{
  ARG_UNUSED(fixture);

  k_event_init(&button_events);
  k_event_clear(&button_events, BUTTON_EVENT);

  LED_STATE = LED_OFF;

  if (main_is_running) {
    k_thread_abort(student_main_tid);
    k_msleep(50);
  }
}

/**
 * @brief Test fixture teardown
 */
void test_after(void *fixture)
{
  ARG_UNUSED(fixture);

  if (main_is_running) {
    k_thread_abort(student_main_tid);
    k_msleep(50);
  }
}