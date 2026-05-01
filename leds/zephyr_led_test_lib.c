#include "bme554_lib.h"

K_EVENT_DEFINE(program_test_events);

/* ------------------------------------------------------------------ */
/*  TESTING HELPER FUNCTIONS: Fixture                                 */
/* ------------------------------------------------------------------ */

static void before(void *)
{
    stop_main();  /* abort any leftover thread from the previous test */
    
    k_event_clear(&program_test_events, FREQ_UP_TEST_NOTICE);
    k_event_clear(&program_test_events, FREQ_DOWN_TEST_NOTICE);
    k_event_clear(&program_test_events, RESET_BTN_TEST_NOTICE);
    k_event_clear(&program_test_events, SLEEP_BTN_TEST_NOTICE);
    k_event_clear(&program_test_events, ERROR_TEST_NOTICE);
    k_event_clear(&program_test_events, RESET_TEST_NOTICE);
    k_event_clear(&program_test_events, SLEEP_TEST_NOTICE);
}

static void after(void *)
{
    stop_main();
    k_msleep(50);
}

/* ------------------------------------------------------------------ */
/*  TESTING HELPER FUNCTIONS: Thread                                  */
/* ------------------------------------------------------------------ */

static void student_main_entry(void *, void *, void *)
{
    main_running = true;
    student_main();
    main_running = false;
}

/** Kill the background thread cleanly. */
static void stop_main(void)
{
    if (main_running) {
        simulate_button_click(&reset_button);
        k_thread_abort(student_main_tid);
        k_msleep(20);
        main_running = false;
    }
}

/**
 * Start student_main() in a background thread.
 *
 * @param settle_ms  How long to wait after spawning before returning.
 *                   150 ms is enough for INIT to run and reach BLINKING_RUN.
 */
static void start_main(int settle_ms)
{
    /* Spawn student thread */
    student_main_tid = k_thread_create(
        &student_main_thread,
        student_main_stack,
        K_THREAD_STACK_SIZEOF(student_main_stack),
        student_main_entry,
        NULL, NULL, NULL,
        STUDENT_MAIN_PRIORITY, 0, K_NO_WAIT);

    k_msleep(settle_ms);
}

/* ------------------------------------------------------------------ */
/*  TESTING HELPER FUNCTIONS: Helpers                                 */
/* ------------------------------------------------------------------ */

static void led_edge_freq_callback(const struct device *dev,
                              struct gpio_callback *cb,
                              uint32_t pins)
{
    g_led_toggles++;
}

static void assert_led_blink_freq(const struct gpio_dt_spec *led,
                                  int window_ms,
                                  int expected_hz,
                                  int tolerance_hz,
                                  const char *led_name)
{
    g_led_toggles = 0;  // reset counter

    struct gpio_callback cb;
    gpio_init_callback(&cb, led_edge_freq_callback, BIT(led->pin));

    int ret = gpio_add_callback_dt(led, &cb);
    zassert_true(ret == 0, "LED %s: failed to add callback", led_name);

    ret = gpio_pin_interrupt_configure_dt(led, GPIO_INT_EDGE_BOTH);
    zassert_true(ret == 0, "LED %s: failed to configure interrupt", led_name);

    k_msleep(window_ms);  // wait measurement window

    ret = gpio_pin_interrupt_configure_dt(led, GPIO_INT_DISABLE);
    zassert_true(ret == 0, "LED %s: failed to disable interrupt", led_name);

    gpio_remove_callback_dt(led, &cb);

    int measured_hz = (g_led_toggles * 500) / window_ms;
    // measuring two edges (toggles) per cycle
    // Hz = (toggels/2)/(window*1000) = (toggles*500)/window

    zassert_within(measured_hz, expected_hz, tolerance_hz,
        "LED %s: expected ~%d Hz but measured ~%d Hz (%d toggles in %d ms)",
        led_name, expected_hz, measured_hz, g_led_toggles, window_ms);
}

/* Assert that an LED is OFF */
static void assert_led_off(const struct gpio_dt_spec *led, const char *led_name)
{
    int val = gpio_emul_output_get(led->port, led->pin);
    zassert_equal(val, 0,
        "Expected LED %s on pin %d to be OFF, but it is ON",
        led_name, led->pin);
}

/* Assert that an LED is ON */
static void assert_led_on(const struct gpio_dt_spec *led, const char *led_name)
{
    int val = gpio_emul_output_get(led->port, led->pin);
    zassert_equal(val, 1,
        "Expected LED %s on pin %d to be ON, but it is OFF",
        led_name, led->pin);
}

/* Return bool LED is ON */
static bool is_led_on(const struct gpio_dt_spec *led)
{
    return gpio_emul_output_get(led->port, led->pin) == 1;
}

/* Assert heartbeat duty cycle */
static void assert_led_duty_cycle(const struct gpio_dt_spec *led,
                                  const char *name,
                                  int window_ms,
                                  int expected_duty,
                                  int tolerance)
{
    struct gpio_dt_spec *led = &blinker_led;
    char *name = "blinker";
    
    struct gpio_callback cb;

    ctx.led = led;
    ctx.on_time = 0;
    ctx.total_time = 0;

    ctx.last_state = gpio_emul_output_get(led->port, led->pin);
    ctx.last_ts = k_uptime_get();

    gpio_init_callback(&cb, led_edge_duty_callback, BIT(led->pin));

    int ret = gpio_add_callback_dt(led, &cb);
    zassert_true(ret == 0, "LED %s: callback add failed", "name");

    ret = gpio_pin_interrupt_configure_dt(led, GPIO_INT_EDGE_BOTH);
    zassert_true(ret == 0, "LED %s: interrupt config failed", name);

    k_msleep(window_ms);

    gpio_pin_interrupt_configure_dt(led, GPIO_INT_DISABLE);
    gpio_remove_callback_dt(led, &cb);

    zassert_true(ctx.total_time > 0,
        "LED %s: no activity detected", name);

    float measured_duty = (float)ctx.on_time / (float)ctx.total_time;

    zassert_true(
        measured_duty > (expected_duty - tolerance) &&
        measured_duty < (expected_duty + tolerance),
        "LED %s: duty %.2f (expected %.2f ± %.2f)",
        name,
        (double)measured_duty,
        (double)expected_duty,
        (double)tolerance
    );
}