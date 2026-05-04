#ifndef ZEPHYR_ADC_EMUL_LIB_H
#define ZEPHYR_ADC_EMUL_LIB_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/adc/adc_emul.h>
#include <zephyr/ztest.h>

/* ------------------------------------------------------------------ */
/*  Constants — defined FIRST so macros below can use them            */
/* ------------------------------------------------------------------ */
#define STUDENT_MAIN_STACK_SIZE  4096
#define STUDENT_MAIN_PRIORITY    5

#define AIN0_CHANNEL_ID          0
#define ADC_EMUL_NODE            DT_NODELABEL(adc_emul)
#define BLINKING_TIME_MS         5000

/* ------------------------------------------------------------------ */
/*  Event bits                                                         */
/* ------------------------------------------------------------------ */
#define FREQ_UP_TEST_NOTICE         BIT(0)
#define FREQ_DOWN_TEST_NOTICE       BIT(1)
#define RESET_BTN_TEST_NOTICE       BIT(2)
#define SLEEP_BTN_TEST_NOTICE       BIT(3)
#define ERROR_TEST_NOTICE           BIT(4)
#define RESET_TEST_NOTICE           BIT(5)
#define SLEEP_TEST_NOTICE           BIT(6)

#define ADC_READ_TRIGGERED_NOTICE   BIT(7)
#define ADC_READ_COMPLETE_NOTICE    BIT(8)
#define ADC_BLINK_DONE_NOTICE       BIT(9)
#define ADC_SAMPLE_TRIGGERED_NOTICE BIT(10)
#define ADC_SAMPLE_COMPLETE_NOTICE  BIT(11)
#define ADC_CYCLES_COMPUTED_NOTICE  BIT(12)
#define ADC_ASYNC_DONE_NOTICE       BIT(13)
#define ADC_ASYNC_TIMEOUT_NOTICE    BIT(14)

/* ------------------------------------------------------------------ */
/*  Shared kernel objects and variables                                */
/* ------------------------------------------------------------------ */
extern struct k_event program_test_events;
extern int            student_adc_mv;
extern float          student_mapped_freq;
extern int            student_calc_cycles_result;
extern int            student_frequency;

/* adc_emul_dev is set in before() and used in tests */
extern const struct device *adc_emul_dev;

/* ------------------------------------------------------------------ */
/*  Student GPIO pins — defined in main.c                             */
/* ------------------------------------------------------------------ */
extern const struct gpio_dt_spec read_button;
extern const struct gpio_dt_spec sleep_button;
extern const struct gpio_dt_spec reset_button;
extern const struct gpio_dt_spec heartbeat_led;
extern const struct gpio_dt_spec blinker_led;
extern const struct gpio_dt_spec error_led;

/* ------------------------------------------------------------------ */
/*  Student main — renamed by CMake -Dmain=student_main               */
/* ------------------------------------------------------------------ */
extern int student_main(void);

/* ------------------------------------------------------------------ */
/*  Thread boilerplate — DEFINE in .c, DECLARE here                   */
/*  K_THREAD_STACK_DECLARE must come AFTER STUDENT_MAIN_STACK_SIZE    */
/* ------------------------------------------------------------------ */
extern struct k_thread student_main_thread;
extern k_tid_t         student_main_tid;
extern volatile bool   main_running;
K_THREAD_STACK_DECLARE(student_main_stack, STUDENT_MAIN_STACK_SIZE);

/* ------------------------------------------------------------------ */
/*  Instrumentation macros — used in main.c                           */
/* ------------------------------------------------------------------ */
#define ADC_READ_TRIGGERED() \
    k_event_post(&program_test_events, ADC_READ_TRIGGERED_NOTICE)

#define ADC_READ_COMPLETE(mv, freq)                         \
    do {                                                    \
        student_adc_mv      = (mv);                         \
        student_mapped_freq = (freq);                       \
        k_event_post(&program_test_events,                  \
                     ADC_READ_COMPLETE_NOTICE);              \
    } while (0)

#define ADC_BLINK_COMPLETE() \
    k_event_post(&program_test_events, ADC_BLINK_DONE_NOTICE)

#define ADC_SAMPLE_TRIGGERED() \
    k_event_post(&program_test_events, ADC_SAMPLE_TRIGGERED_NOTICE)

#define ADC_SAMPLE_COMPLETE() \
    k_event_post(&program_test_events, ADC_SAMPLE_COMPLETE_NOTICE)

#define ADC_CYCLES_COMPUTED(cycles)                         \
    do {                                                    \
        student_calc_cycles_result = (cycles);              \
        k_event_post(&program_test_events,                  \
                     ADC_CYCLES_COMPUTED_NOTICE);            \
    } while (0)

#define ADC_ASYNC_COMPLETE() \
    k_event_post(&program_test_events, ADC_ASYNC_DONE_NOTICE)

#define ADC_ASYNC_TIMED_OUT() \
    k_event_post(&program_test_events, ADC_ASYNC_TIMEOUT_NOTICE)

#define RESET_PRESSED() \
    k_event_post(&program_test_events, RESET_TEST_NOTICE)

#define RESET_STATUS() \
    k_event_post(&program_test_events, RESET_TEST_NOTICE)

#define SLEEP_PRESSED() \
    k_event_post(&program_test_events, SLEEP_BTN_TEST_NOTICE)

#define SLEEP_STATE() \
    k_event_post(&program_test_events, SLEEP_TEST_NOTICE)

#define ERROR_STATE() \
    k_event_post(&program_test_events, ERROR_TEST_NOTICE)

/* ------------------------------------------------------------------ */
/*  Test helper function declarations                                  */
/* ------------------------------------------------------------------ */
void before(void *);
void after(void *);
void student_main_entry(void *, void *, void *);
void stop_main(void);
void start_main(int settle_ms);
bool wait_for_event(uint32_t mask, int timeout_ms);
void simulate_button_click(const struct gpio_dt_spec *button);
void assert_led_off(const struct gpio_dt_spec *led, const char *led_name);
void assert_led_on(const struct gpio_dt_spec *led,  const char *led_name);
void set_ain0_mv(const struct device *dev, int millivolts);
void assert_led_blink_freq(const struct gpio_dt_spec *led, int window_ms,
                            int expected_hz, int tolerance_hz,
                            const char *led_name);
void assert_blink_ontime_pct(int window_ms, int expected_duty, int tolerance);
void assert_blink_total_duration_ms(int expected_ms, int tolerance_ms);

#endif /* ZEPHYR_ADC_EMUL_LIB_H */