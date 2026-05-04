#include "zephyr_adc_emul_lib.h"

#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/drivers/adc/adc_emul.h>
#include <zephyr/ztest.h>

/* ------------------------------------------------------------------ */
/*  Kernel objects — declared extern in .h, defined here              */
/* ------------------------------------------------------------------ */
K_EVENT_DEFINE(program_test_events);

int   student_adc_mv          = 0;
float student_mapped_freq     = 0.0f;
int   student_calc_cycles_result = 0;
int   student_frequency       = 0;

const struct device *adc_emul_dev;

/* ------------------------------------------------------------------ */
/*  Thread state                                                       */
/* ------------------------------------------------------------------ */
K_THREAD_STACK_DEFINE(student_main_stack, STUDENT_MAIN_STACK_SIZE);
struct k_thread student_main_thread;
k_tid_t         student_main_tid;
volatile bool   main_running = false;

/* ------------------------------------------------------------------ */
/*  Internal state for LED measurement helpers                        */
/* ------------------------------------------------------------------ */
static volatile int g_led_toggles = 0;

struct duty_ctx {
    const struct gpio_dt_spec *led;
    int64_t last_ts;
    bool    last_state;
    int64_t on_time;
    int64_t total_time;
};
static struct duty_ctx ctx;

/* ------------------------------------------------------------------ */
/*  Fixture                                                            */
/* ------------------------------------------------------------------ */
void before(void *)
{
    stop_main();

    gpio_emul_input_set(read_button.port,  read_button.pin,  0);
    gpio_emul_input_set(sleep_button.port, sleep_button.pin, 0);
    gpio_emul_input_set(reset_button.port, reset_button.pin, 0);
    k_msleep(50);

    adc_emul_dev = DEVICE_DT_GET(ADC_EMUL_NODE);
    zassert_true(device_is_ready(adc_emul_dev), "ADC emulator not ready");

    start_main(500);

    k_event_clear(&program_test_events,
        ADC_READ_TRIGGERED_NOTICE | ADC_READ_COMPLETE_NOTICE |
        ADC_BLINK_DONE_NOTICE     |
        ADC_SAMPLE_TRIGGERED_NOTICE | ADC_SAMPLE_COMPLETE_NOTICE);
}

void after(void *)
{
    stop_main();
    k_msleep(50);
}

/* ------------------------------------------------------------------ */
/*  Thread helpers                                                     */
/* ------------------------------------------------------------------ */
void student_main_entry(void *, void *, void *)
{
    main_running = true;
    student_main();
    main_running = false;
}

void stop_main(void)
{
    if (main_running) {
        k_thread_abort(student_main_tid);
        k_msleep(20);
        main_running = false;
    }
}

void start_main(int settle_ms)
{
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
/*  Event helpers                                                      */
/* ------------------------------------------------------------------ */
bool wait_for_event(uint32_t mask, int timeout_ms)
{
    int64_t  start = k_uptime_get();
    uint32_t events = 0;

    do {
        events = k_event_wait(&program_test_events, mask, false, K_MSEC(20));
        if (events & mask) {
            return true;
        }
    } while ((k_uptime_get() - start) < timeout_ms);

    return false;
}

/* ------------------------------------------------------------------ */
/*  Button helper                                                      */
/* ------------------------------------------------------------------ */
void simulate_button_click(const struct gpio_dt_spec *button)
{
    gpio_emul_input_set(button->port, button->pin, 1);
    k_sleep(K_MSEC(5));
    gpio_emul_input_set(button->port, button->pin, 0);
}

/* ------------------------------------------------------------------ */
/*  LED state helpers                                                  */
/* ------------------------------------------------------------------ */
void assert_led_off(const struct gpio_dt_spec *led, const char *led_name)
{
    int val = gpio_emul_output_get(led->port, led->pin);
    zassert_equal(val, 0,
        "Expected LED %s (pin %d) to be OFF but it is ON",
        led_name, led->pin);
}

void assert_led_on(const struct gpio_dt_spec *led, const char *led_name)
{
    int val = gpio_emul_output_get(led->port, led->pin);
    zassert_equal(val, 1,
        "Expected LED %s (pin %d) to be ON but it is OFF",
        led_name, led->pin);
}

/* ------------------------------------------------------------------ */
/*  ADC emulator helper                                                */
/* ------------------------------------------------------------------ */
void set_ain0_mv(const struct device *dev, int millivolts)
{
    int ret = adc_emul_const_value_set(dev, AIN0_CHANNEL_ID, millivolts);
    zassert_ok(ret, "adc_emul_const_value_set failed (%d)", ret);
}

/* ------------------------------------------------------------------ */
/*  LED frequency measurement                                          */
/* ------------------------------------------------------------------ */
static void led_edge_callback(const struct device *dev,
                               struct gpio_callback *cb,
                               uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);
    g_led_toggles++;
}

void assert_led_blink_freq(const struct gpio_dt_spec *led,
                            int window_ms,
                            int expected_hz,
                            int tolerance_hz,
                            const char *led_name)
{
    g_led_toggles = 0;

    struct gpio_callback cb;
    gpio_init_callback(&cb, led_edge_callback, BIT(led->pin));

    int ret = gpio_add_callback_dt(led, &cb);
    zassert_true(ret == 0, "LED %s: failed to add callback", led_name);

    ret = gpio_pin_interrupt_configure_dt(led, GPIO_INT_EDGE_BOTH);
    zassert_true(ret == 0, "LED %s: failed to configure interrupt", led_name);

    k_msleep(window_ms);

    gpio_pin_interrupt_configure_dt(led, GPIO_INT_DISABLE);
    gpio_remove_callback_dt(led, &cb);

    int measured_hz = (g_led_toggles * 500) / window_ms;

    zassert_within(measured_hz, expected_hz, tolerance_hz,
        "LED %s: expected ~%d Hz but measured ~%d Hz (%d toggles in %d ms)",
        led_name, expected_hz, measured_hz, g_led_toggles, window_ms);
}

/* ------------------------------------------------------------------ */
/*  LED duty cycle measurement                                         */
/* ------------------------------------------------------------------ */
static void led_edge_duty_callback(const struct device *dev,
                                    struct gpio_callback *cb,
                                    uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    int64_t now   = k_uptime_get();
    int64_t delta = now - ctx.last_ts;

    if (ctx.last_state) {
        ctx.on_time += delta;
    }
    ctx.total_time += delta;
    ctx.last_state = !ctx.last_state;
    ctx.last_ts    = now;
}

void assert_blink_ontime_pct(int window_ms, int expected_duty, int tolerance)
{
    const struct gpio_dt_spec *led = &blinker_led;
    const char *name = "blinker";

    struct gpio_callback cb;

    ctx.on_time    = 0;
    ctx.total_time = 0;
    ctx.last_state = gpio_emul_output_get(led->port, led->pin);
    ctx.last_ts    = k_uptime_get();

    gpio_init_callback(&cb, led_edge_duty_callback, BIT(led->pin));

    int ret = gpio_add_callback_dt(led, &cb);
    zassert_true(ret == 0, "LED %s: callback add failed", name);

    ret = gpio_pin_interrupt_configure_dt(led, GPIO_INT_EDGE_BOTH);
    zassert_true(ret == 0, "LED %s: interrupt config failed", name);

    k_msleep(window_ms);

    gpio_pin_interrupt_configure_dt(led, GPIO_INT_DISABLE);
    gpio_remove_callback_dt(led, &cb);

    zassert_true(ctx.total_time > 0, "LED %s: no activity detected", name);

    float measured_duty = ((float)ctx.on_time / (float)ctx.total_time) * 100.0f;

    zassert_true(
        measured_duty > (expected_duty - tolerance) &&
        measured_duty < (expected_duty + tolerance),
        "LED %s: duty %.2f%% (expected %d%% ± %d%%)",
        name, (double)measured_duty, expected_duty, tolerance);
}

/* ------------------------------------------------------------------ */
/*  Blink total duration measurement                                   */
/* ------------------------------------------------------------------ */
void assert_blink_total_duration_ms(int expected_ms, int tolerance_ms)
{
    g_led_toggles = 0;

    struct gpio_callback cb;
    gpio_init_callback(&cb, led_edge_callback, BIT(blinker_led.pin));
    gpio_add_callback_dt(&blinker_led, &cb);
    gpio_pin_interrupt_configure_dt(&blinker_led, GPIO_INT_EDGE_BOTH);

    /* Wait for first edge */
    int64_t t_wait = k_uptime_get();
    while (g_led_toggles == 0 && (k_uptime_get() - t_wait) < 500) {
        k_msleep(5);
    }
    int64_t t_start = k_uptime_get();

    gpio_pin_interrupt_configure_dt(&blinker_led, GPIO_INT_DISABLE);
    gpio_remove_callback_dt(&blinker_led, &cb);

    zassert_true(g_led_toggles > 0,
        "blinker never toggled, can't measure duration");

    uint32_t events = k_event_wait(&program_test_events,
                                   ADC_BLINK_DONE_NOTICE,
                                   true,
                                   K_MSEC(expected_ms + tolerance_ms + 500));
    zassert_true(events & ADC_BLINK_DONE_NOTICE,
        "ADC_BLINK_DONE_NOTICE never fired within timeout");

    int64_t measured_ms = k_uptime_get() - t_start;

    zassert_within((int)measured_ms, expected_ms, tolerance_ms,
        "blink duration: expected ~%d ms but measured ~%lld ms",
        expected_ms, measured_ms);
}