#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/smf.h>
#include "zephyr_adc_emul_lib.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

/* ------------------------------------------------------------------ */
/*  Macros                                                             */
/* ------------------------------------------------------------------ */
#define MS_PER_HZ           1000
#define HB_ON_TIME          250
#define MAX_FREQ_HZ         5
#define MIN_FREQ_HZ         1
#define MIN_V_MV            0
#define MAX_V_MV            2990
#define READ_EVENT          BIT(0)
#define SLEEP_EVENT         BIT(1)
#define TIMER_COMPLETE_EVENT BIT(2)
#define RESET_EVENT         BIT(3)

#define ADC_DT_SPEC_GET_BY_ALIAS(adc_alias)                         \
{                                                                    \
    .dev = DEVICE_DT_GET(DT_PARENT(DT_ALIAS(adc_alias))),           \
    .channel_id = DT_REG_ADDR(DT_ALIAS(adc_alias)),                  \
    ADC_CHANNEL_CFG_FROM_DT_NODE(DT_ALIAS(adc_alias))               \
}

/* ------------------------------------------------------------------ */
/*  Hardware structs                                                   */
/* ------------------------------------------------------------------ */
const struct gpio_dt_spec read_button   = GPIO_DT_SPEC_GET(DT_ALIAS(readbutton),  gpios);
const struct gpio_dt_spec sleep_button  = GPIO_DT_SPEC_GET(DT_ALIAS(sleepbutton), gpios);
const struct gpio_dt_spec reset_button  = GPIO_DT_SPEC_GET(DT_ALIAS(resetbutton), gpios);

const struct gpio_dt_spec heartbeat_led = GPIO_DT_SPEC_GET(DT_ALIAS(heartbeat), gpios);
const struct gpio_dt_spec blinker_led   = GPIO_DT_SPEC_GET(DT_ALIAS(blinker),   gpios);
const struct gpio_dt_spec error_led     = GPIO_DT_SPEC_GET(DT_ALIAS(error),     gpios);

static const struct adc_dt_spec adc_vadc = ADC_DT_SPEC_GET_BY_ALIAS(vadc);

/* ------------------------------------------------------------------ */
/*  Callback and timer forward declarations                            */
/* ------------------------------------------------------------------ */
void read_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void sleep_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void reset_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins);

void blinking_interrupt_handler(struct k_timer *blinking_timer);
void led_on_interrupt_handler(struct k_timer *led_on_timer);
void led_off_interrupt_handler(struct k_timer *led_off_timer);
void led_on_stop_handler(struct k_timer *led_on_timer);

void heartbeat_thread(void *, void *, void *);

/* ------------------------------------------------------------------ */
/*  Kernel objects                                                     */
/* ------------------------------------------------------------------ */
K_EVENT_DEFINE(button_events);

static struct gpio_callback read_button_cb;
static struct gpio_callback sleep_button_cb;
static struct gpio_callback reset_button_cb;

K_TIMER_DEFINE(blinking_timer, blinking_interrupt_handler, NULL);
K_TIMER_DEFINE(led_on_timer,   led_on_interrupt_handler,   led_on_stop_handler);
K_TIMER_DEFINE(led_off_timer,  led_off_interrupt_handler,  NULL);

K_THREAD_DEFINE(heartbeat_thread_id, 1024, heartbeat_thread, NULL, NULL, NULL, 5, 0, 0);

/* ------------------------------------------------------------------ */
/*  ADC sequence buffer                                                */
/* ------------------------------------------------------------------ */
int16_t buf;
struct adc_sequence sequence = {
    .buffer      = &buf,
    .buffer_size = sizeof(buf),
};

/* ------------------------------------------------------------------ */
/*  State machine                                                      */
/* ------------------------------------------------------------------ */
struct s_object {
    struct smf_ctx ctx;
    int32_t millivolts;
    float   freq;
    float   ontime;
    float   offtime;
    int64_t starttime;
    int64_t endtime;
};

struct s_object s_context = {
    .millivolts = 0,
    .freq       = 0,
    .ontime     = 0,
    .offtime    = 0,
    .starttime  = 0,
    .endtime    = 0,
};

static void s_init(void *o);
static void s_reset(void *o);
static void idle_entry(void *o);
static void idle_run(void *o);
static void idle_exit(void *o);
static void sleep_run(void *o);
static void reading_entry(void *o);
static void reading_run(void *o);
static void reading_exit(void *o);
static void blinking_entry(void *o);
static void blinking_run(void *o);
static void blinking_exit(void *o);
static void error_entry(void *o);
static void error_run(void *o);
static void error_exit(void *o);

enum state { INIT, RESET, IDLE, SLEEP, READING, BLINKING, ERROR };

static const struct smf_state states[] = {
    [INIT]     = SMF_CREATE_STATE(NULL,        s_init,       NULL,          NULL, NULL),
    [RESET]    = SMF_CREATE_STATE(NULL,        s_reset,      NULL,          NULL, NULL),
    [IDLE]     = SMF_CREATE_STATE(idle_entry,  idle_run,     idle_exit,     NULL, NULL),
    [SLEEP]    = SMF_CREATE_STATE(NULL,        sleep_run,    NULL,          NULL, NULL),
    [READING]  = SMF_CREATE_STATE(reading_entry, reading_run, reading_exit, NULL, NULL),
    [BLINKING] = SMF_CREATE_STATE(blinking_entry, blinking_run, blinking_exit, NULL, NULL),
    [ERROR]    = SMF_CREATE_STATE(error_entry, error_run,    error_exit,    NULL, NULL),
};

/* ------------------------------------------------------------------ */
/*  main                                                               */
/* ------------------------------------------------------------------ */
int main(void)
{
    smf_set_initial(SMF_CTX(&s_context), &states[INIT]);

    while (1) {
        int err = smf_run_state(SMF_CTX(&s_context));
        if (err) {
            smf_set_terminate(SMF_CTX(&s_context), err);
            break;
        }
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/*  Callbacks                                                          */
/* ------------------------------------------------------------------ */
void read_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    ADC_READ_TRIGGERED();
    k_event_post(&button_events, READ_EVENT);
}

void sleep_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    SLEEP_PRESSED();
    k_event_post(&button_events, SLEEP_EVENT);
}

void reset_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    RESET_PRESSED();
    k_event_post(&button_events, RESET_EVENT);
}

/* ------------------------------------------------------------------ */
/*  Heartbeat thread                                                   */
/* ------------------------------------------------------------------ */
void heartbeat_thread(void *, void *, void *)
{
    while (1) {
        k_msleep(HB_ON_TIME);
        gpio_pin_toggle_dt(&heartbeat_led);
        k_msleep(MS_PER_HZ - HB_ON_TIME);
        gpio_pin_toggle_dt(&heartbeat_led);
    }
}

/* ------------------------------------------------------------------ */
/*  Timer handlers                                                     */
/* ------------------------------------------------------------------ */
void blinking_interrupt_handler(struct k_timer *timer)
{
    s_context.endtime = k_uptime_get();
    ADC_BLINK_COMPLETE();
    k_event_post(&button_events, TIMER_COMPLETE_EVENT);
}

void led_on_interrupt_handler(struct k_timer *timer)
{
    gpio_pin_set_dt(&blinker_led, 0);
    k_timer_start(&led_off_timer, K_MSEC(s_context.offtime), K_NO_WAIT);
}

void led_off_interrupt_handler(struct k_timer *timer)
{
    gpio_pin_set_dt(&blinker_led, 1);
    k_timer_start(&led_on_timer, K_MSEC(s_context.ontime), K_NO_WAIT);
}

void led_on_stop_handler(struct k_timer *timer)
{
    gpio_pin_set_dt(&blinker_led, 0);
}

/* ------------------------------------------------------------------ */
/*  State functions                                                    */
/* ------------------------------------------------------------------ */
static void s_init(void *o)
{
    int err = 0;

    if (!device_is_ready(read_button.port)) {
        LOG_ERR("GPIO not ready");
        smf_set_terminate(SMF_CTX(&s_context), -1);
        return;
    }
    if (!device_is_ready(adc_vadc.dev)) {
        LOG_ERR("ADC not ready");
        smf_set_terminate(SMF_CTX(&s_context), -1);
        return;
    }

    /* Configure buttons */
    err = gpio_pin_configure_dt(&read_button,  GPIO_INPUT);
    err |= gpio_pin_configure_dt(&sleep_button, GPIO_INPUT);
    err |= gpio_pin_configure_dt(&reset_button, GPIO_INPUT);
    if (err < 0) {
        LOG_ERR("Button configure failed");
        smf_set_terminate(SMF_CTX(&s_context), err);
        return;
    }

    /* Attach interrupts */
    gpio_pin_interrupt_configure_dt(&read_button,  GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&read_button_cb, read_button_callback, BIT(read_button.pin));
    gpio_add_callback_dt(&read_button, &read_button_cb);

    gpio_pin_interrupt_configure_dt(&sleep_button, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&sleep_button_cb, sleep_button_callback, BIT(sleep_button.pin));
    gpio_add_callback_dt(&sleep_button, &sleep_button_cb);

    gpio_pin_interrupt_configure_dt(&reset_button, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&reset_button_cb, reset_button_callback, BIT(reset_button.pin));
    gpio_add_callback_dt(&reset_button, &reset_button_cb);

    /* Configure LEDs */
    gpio_pin_configure_dt(&heartbeat_led, GPIO_OUTPUT_ACTIVE);
    gpio_pin_configure_dt(&blinker_led,   GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&error_led,     GPIO_OUTPUT_INACTIVE);

    /* Configure ADC */
    err = adc_channel_setup_dt(&adc_vadc);
    if (err < 0) {
        LOG_ERR("ADC channel setup failed (%d)", err);
        smf_set_terminate(SMF_CTX(&s_context), err);
        return;
    }

    smf_set_state(SMF_CTX(&s_context), &states[IDLE]);
}

static void s_reset(void *o)
{
    RESET_STATUS();
    smf_set_state(SMF_CTX(&s_context), &states[IDLE]);
}

static void idle_entry(void *o)
{
    gpio_pin_interrupt_configure_dt(&read_button, GPIO_INT_EDGE_TO_ACTIVE);
}

static void idle_run(void *o)
{
    uint32_t events = k_event_wait(&button_events,
                                   READ_EVENT | SLEEP_EVENT | RESET_EVENT,
                                   true, K_FOREVER);
    if (events & READ_EVENT) {
        smf_set_state(SMF_CTX(&s_context), &states[READING]);
    } else if (events & SLEEP_EVENT) {
        smf_set_state(SMF_CTX(&s_context), &states[SLEEP]);
    } else if (events & RESET_EVENT) {
        smf_set_state(SMF_CTX(&s_context), &states[RESET]);
    }
}

static void idle_exit(void *o)
{
    gpio_pin_interrupt_configure_dt(&read_button, GPIO_INT_DISABLE);
}

static void sleep_run(void *o)
{
    SLEEP_STATE();

    uint32_t events = k_event_wait(&button_events,
                                   SLEEP_EVENT | RESET_EVENT,
                                   true, K_FOREVER);
    if (events & SLEEP_EVENT) {
        smf_set_state(SMF_CTX(&s_context), &states[IDLE]);
    } else if (events & RESET_EVENT) {
        smf_set_state(SMF_CTX(&s_context), &states[RESET]);
    }
}

static void reading_entry(void *o)
{
    gpio_pin_interrupt_configure_dt(&sleep_button, GPIO_INT_DISABLE);
    gpio_pin_interrupt_configure_dt(&reset_button, GPIO_INT_DISABLE);
    adc_sequence_init_dt(&adc_vadc, &sequence);
}

static void reading_run(void *o)
{
    int ret = adc_read(adc_vadc.dev, &sequence);
    if (ret < 0) {
        LOG_ERR("ADC read failed (%d)", ret);
        smf_set_state(SMF_CTX(&s_context), &states[ERROR]);
        return;
    }

    int32_t val_mv = buf;
    ret = adc_raw_to_millivolts_dt(&adc_vadc, &val_mv);
    if (ret < 0) {
        LOG_WRN("raw→mV conversion failed, using raw value");
    }

    LOG_INF("ADC: %d mV", val_mv);
    s_context.millivolts = val_mv;

    float max_v = MAX_V_MV;
    s_context.freq = ((float)s_context.millivolts *
                      (MAX_FREQ_HZ - MIN_FREQ_HZ)) / max_v + MIN_FREQ_HZ;

    float period       = MS_PER_HZ / s_context.freq;
    s_context.ontime   = (int)(period * 0.1f);
    s_context.offtime  = (int)(period - s_context.ontime);

    LOG_INF("Freq: %.2f Hz  on: %.0f ms  off: %.0f ms",
            (double)s_context.freq,
            (double)s_context.ontime,
            (double)s_context.offtime);

    ADC_READ_COMPLETE(s_context.millivolts, s_context.freq);

    if (val_mv < MIN_V_MV || val_mv > MAX_V_MV) {
        smf_set_state(SMF_CTX(&s_context), &states[ERROR]);
    } else {
        smf_set_state(SMF_CTX(&s_context), &states[BLINKING]);
    }
}

static void reading_exit(void *o)
{
    gpio_pin_interrupt_configure_dt(&sleep_button, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_pin_interrupt_configure_dt(&reset_button, GPIO_INT_EDGE_TO_ACTIVE);
}

static void blinking_entry(void *o)
{
    gpio_pin_set_dt(&blinker_led, 1);
    gpio_pin_interrupt_configure_dt(&read_button, GPIO_INT_DISABLE);

    k_timer_start(&led_on_timer,    K_MSEC(s_context.ontime),  K_NO_WAIT);
    k_timer_start(&blinking_timer,  K_MSEC(BLINKING_TIME_MS),  K_NO_WAIT);

    s_context.starttime = k_uptime_get();
}

static void blinking_run(void *o)
{
    uint32_t events = k_event_wait(&button_events,
                                   TIMER_COMPLETE_EVENT | SLEEP_EVENT | RESET_EVENT,
                                   true, K_FOREVER);
    if (events & TIMER_COMPLETE_EVENT) {
        smf_set_state(SMF_CTX(&s_context), &states[IDLE]);
    } else if (events & SLEEP_EVENT) {
        smf_set_state(SMF_CTX(&s_context), &states[SLEEP]);
    } else if (events & RESET_EVENT) {
        smf_set_state(SMF_CTX(&s_context), &states[RESET]);
    }
}

static void blinking_exit(void *o)
{
    k_timer_stop(&blinking_timer);
    k_timer_stop(&led_on_timer);
    k_timer_stop(&led_off_timer);
    gpio_pin_set_dt(&blinker_led, 0);
    gpio_pin_interrupt_configure_dt(&read_button, GPIO_INT_EDGE_TO_ACTIVE);

    LOG_INF("Blink ran for %lld ms",
            s_context.endtime - s_context.starttime);
}

static void error_entry(void *o)
{
    ERROR_STATE();
    gpio_pin_interrupt_configure_dt(&sleep_button, GPIO_INT_DISABLE);
    gpio_pin_set_dt(&error_led, 1);
}

static void error_run(void *o)
{
    uint32_t events = k_event_wait(&button_events,
                                   RESET_EVENT, true, K_FOREVER);
    if (events & RESET_EVENT) {
        smf_set_state(SMF_CTX(&s_context), &states[RESET]);
    }
}

static void error_exit(void *o)
{
    gpio_pin_interrupt_configure_dt(&sleep_button, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_pin_set_dt(&error_led, 0);
}