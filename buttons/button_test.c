#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "button_test.h"
#include "zephyr_button_test_lib.h"

/* Thread for running student's main code */
#define STUDENT_MAIN_STACK_SIZE 1024
#define STUDENT_MAIN_PRIORITY 5
// K_THREAD_STACK_DEFINE(student_main_stack, STUDENT_MAIN_STACK_SIZE);

bool wait_for_event(uint32_t mask, int timeout_ms)
{
    int64_t start = k_uptime_get();
    uint32_t events = 0;

    do {
        events = k_event_wait(&button_events, mask, false, K_MSEC(20));
        if (events & mask) {
            return true;
        }
    } while ((k_uptime_get() - start) < timeout_ms);

    return false;
}


/**
 * @brief Test that callback posts BUTTON_EVENT when called
 */
ZTEST(button_press_tests, test_callback_posts_event)
{
    k_event_clear(&button_events, BUTTON_EVENT1 | BUTTON_EVENT2);
    start_main(1000);
    
    uint32_t events_before = k_event_wait(&button_events, BUTTON_EVENT1, false, K_NO_WAIT);
    zassert_false(events_before & BUTTON_EVENT1, "BUTTON_EVENT should not be set initially");

    simulate_button_click(&button_test);
    k_msleep(50);

    uint32_t events_after = k_event_wait(&button_events, BUTTON_EVENT2, false, K_NO_WAIT);
    zassert_true(events_after & BUTTON_EVENT2, "Callback should post BUTTON_EVENT");
}

/**
 * @brief Test callback can be called multiple times
 */
ZTEST(button_press_tests, test_callback_multiple_calls)
{
    k_event_clear(&button_events, BUTTON_EVENT1 | BUTTON_EVENT2);
    start_main(1000);
    
    /* First press */
    simulate_button_click(&button_test);
    k_msleep(50);
    
    // uint32_t events1 = k_event_wait(&button_events, BUTTON_EVENT1, false, K_NO_WAIT);
    // zassert_true(events1 & BUTTON_EVENT1, "First callback should post event");

    zassert_true(wait_for_event(BUTTON_EVENT1, 800), "First callback should post event");
    
    /* Press again */
    simulate_button_click(&button_test);
    k_msleep(50);
    
    uint32_t events2 = k_event_wait(&button_events, BUTTON_EVENT2, false, K_NO_WAIT);
    zassert_true(events2 & BUTTON_EVENT2, "Second callback should also post event");
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
    k_event_clear(&button_events, BUTTON_EVENT1 | BUTTON_EVENT2);
    start_main(1000);
    
    /* Start with LED OFF */
    LED_STATE = LED_OFF;
    int initial_state = LED_STATE;
    
    zassert_equal(LED_STATE, initial_state, "LED should not change before button press");
    
    /* Simulate button press */
    simulate_button_click(&button_test);
    
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
    k_event_clear(&button_events, BUTTON_EVENT1 | BUTTON_EVENT2);
    start_main(1000);
    
    /* Start with LED OFF */
    LED_STATE = LED_OFF;
    
    /* First press - should turn LED ON */
    simulate_button_click(&button_test);
    k_msleep(100);
    
    int state_after_first = LED_STATE;
    zassert_equal(state_after_first, LED_ON, "LED should be ON after first press");
    
    /* Second press - should turn LED OFF */
    simulate_button_click(&button_test);
    k_msleep(100);
    
    int state_after_second = LED_STATE;
    zassert_equal(state_after_second, LED_OFF, "LED should be OFF after second press");
}

ZTEST_SUITE(button_press_tests, NULL, NULL, before, after, NULL);