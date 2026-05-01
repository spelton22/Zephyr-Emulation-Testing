#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "button_test.h"
#include "zephyr_button_test_support.h"

/* Thread for running student's main code */
#define STUDENT_MAIN_STACK_SIZE 1024
#define STUDENT_MAIN_PRIORITY 5
// K_THREAD_STACK_DEFINE(student_main_stack, STUDENT_MAIN_STACK_SIZE);

/**
 * @brief Test that callback posts BUTTON_EVENT when called
 */
ZTEST(button_press_tests, test_callback_posts_event)
{
  uint32_t events_before = k_event_wait(&button_events, BUTTON_EVENT, false, K_NO_WAIT);
  zassert_equal(events_before, 0, "BUTTON_EVENT should not be set initially");

  button_test_callback(NULL, NULL, BIT(0));
  k_msleep(5);

  uint32_t events_after = k_event_wait(&button_events, BUTTON_EVENT, false, K_NO_WAIT);
  zassert_true(events_after & BUTTON_EVENT, "Callback should post BUTTON_EVENT");
}

/**
 * @brief Test callback can be called multiple times
 */
ZTEST(button_press_tests, test_callback_multiple_calls)
{
  /* First press */
  button_test_callback(NULL, NULL, BIT(0));
  k_msleep(5);
  
  uint32_t events1 = k_event_wait(&button_events, BUTTON_EVENT, false, K_NO_WAIT);
  zassert_true(events1 & BUTTON_EVENT, "First callback should post event");
  
  /* Clear and press again */
  k_event_clear(&button_events, BUTTON_EVENT);
  button_test_callback(NULL, NULL, BIT(0));
  k_msleep(5);
  
  uint32_t events2 = k_event_wait(&button_events, BUTTON_EVENT, false, K_NO_WAIT);
  zassert_true(events2 & BUTTON_EVENT, "Second callback should also post event");
}

/**
 * @brief Test that student_main responds to first button press
 * 
 * This tests the complete flow:
 * 1. Start student_main (which waits for button press)
 * 2. Simulate button press via callback
 * 3. Verify LED_STATE toggles
 */
ZTEST(button_press_tests, test_main_responds_to_first_press)
{
  /* Start with LED OFF */
  LED_STATE = LED_OFF;
  int initial_state = LED_STATE;
  
  /* Start student_main in background thread */
  student_main_tid = k_thread_create(&student_main_thread,
                                      student_main_stack,
                                      K_THREAD_STACK_SIZEOF(student_main_stack),
                                      student_main_thread_entry,
                                      NULL, NULL, NULL,
                                      STUDENT_MAIN_PRIORITY, 0, K_NO_WAIT);
  
  k_msleep(50);
  
  zassert_equal(LED_STATE, initial_state, "LED should not change before button press");
  
  /* Simulate button press */
  button_test_callback(NULL, NULL, BIT(0));
  
  k_msleep(100);
  
  zassert_not_equal(LED_STATE, initial_state,
                    "LED_STATE should toggle after button press (was %d, now %d)",
                    initial_state, LED_STATE);
}

/**
 * @brief Test that student_main responds to second button press
 * 
 * Tests the complete flow with two presses to ensure LED toggles correctly
 */
ZTEST(button_press_tests, test_main_responds_to_second_press)
{
  /* Start with LED OFF */
  LED_STATE = LED_OFF;
  
  /* Start student_main */
  student_main_tid = k_thread_create(&student_main_thread,
                                      student_main_stack,
                                      K_THREAD_STACK_SIZEOF(student_main_stack),
                                      student_main_thread_entry,
                                      NULL, NULL, NULL,
                                      STUDENT_MAIN_PRIORITY, 0, K_NO_WAIT);
  
  k_msleep(50);
  
  /* First press - should turn LED ON */
  button_test_callback(NULL, NULL, BIT(0));
  k_msleep(100);
  
  int state_after_first = LED_STATE;
  zassert_equal(state_after_first, LED_ON, "LED should be ON after first press");
  
  /* Second press - should turn LED OFF */
  button_test_callback(NULL, NULL, BIT(0));
  k_msleep(100);
  
  int state_after_second = LED_STATE;
  zassert_equal(state_after_second, LED_OFF, "LED should be OFF after second press");
}

ZTEST_SUITE(button_press_tests, NULL, NULL, test_before, test_after, NULL);