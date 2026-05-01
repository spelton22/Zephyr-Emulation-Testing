#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h> 

#define LED_TOGGLE_MS 500

// GPIOs
const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sleepbutton), gpios);
const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(heartbeat), gpios);

/* Event object */
K_EVENT_DEFINE(program_events);

/* Event bits */
#define EVENT_LED_ON      BIT(0)
#define EVENT_LED_OFF     BIT(1)
#define EVENT_BUTTON      BIT(2)

/* Button callback structure */
static struct gpio_callback button_cb_data;

void button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    k_event_post(&program_events, EVENT_BUTTON);
}

/* ---------- LED THREAD ---------- */

void led_thread(void)
{
    bool state = false;
    
    while (1) {
        state = !state;
        
        gpio_pin_set_dt(&led, state);
        
        if (state) {
            k_event_post(&program_events, EVENT_LED_ON);
        } else {
            k_event_post(&program_events, EVENT_LED_OFF);
        }
        
        k_msleep(LED_TOGGLE_MS);
    }
}

/* ---------- THREAD DEFINITIONS ---------- */

K_THREAD_DEFINE(led_tid, 1024, led_thread, NULL, NULL, NULL,
                5, 0, 0);

/* ---------- MAIN ---------- */

int main(void)
{
    if (!gpio_is_ready_dt(&led)) {
        printk("LED not ready\n");
        return 0;
    }
    if (!gpio_is_ready_dt(&button)) {
        printk("Button not ready\n");
        return 0;
    }

    /* Configure LED */
    gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);

    /* Configure button */
    gpio_pin_configure_dt(&button, GPIO_INPUT);

    gpio_pin_interrupt_configure_dt(&button,
                                    GPIO_INT_EDGE_TO_ACTIVE);

    gpio_init_callback(&button_cb_data,
                       button_callback,
                       BIT(button.pin));

    gpio_add_callback_dt(&button, &button_cb_data);

    return 0;
}