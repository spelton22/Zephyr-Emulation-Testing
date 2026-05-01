#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "led_test.h"
#include "zephyr_led_test_lib.h"

ZTEST(led_tests, test_led_toggles)
{
    start_main(1000);
    
    bool first = is_led_on(&led);
    k_msleep(1100);  // slightly more than period
    bool second = is_led_on(&led);

    zassert_not_equal(first, second,
                      "LED did not toggle");
}

ZTEST(led_tests, test_blink_frequency)
{
    start_main(1000);
    
    assert_led_blink_freq(&led,
                          4000,   /* observe for 4 seconds */
                          1,      /* expected 1 Hz */
                          1,      /* allow small tolerance */
                          "blinking LED");
}

/* Verify duty cycle ≈ 50% */
ZTEST(led_tests, test_duty_cycle)
{
    start_main(1000);
    
    assert_led_duty_cycle(&led,
                          "blinking LED",
                          4000,   /* observation window */
                          50,     /* expected 50% */
                          10);    /* ±10% tolerance */
}

ZTEST_SUITE(led_tests, NULL, NULL, before, after, NULL);