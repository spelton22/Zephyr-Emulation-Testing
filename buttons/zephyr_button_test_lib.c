#include "zephyr_button_test_lib.h"

#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/ztest.h>
#include <zephyr/ztest.h>
#include <zephyr/kernel.h>

/* ------------------------------------------------------------------ */
/*  TESTING HELPER FUNCTIONS: Fixture                                 */
/* ------------------------------------------------------------------ */

void before(void *)
{
    stop_main();  /* abort any leftover thread from the previous test */
}

void after(void *)
{
    stop_main();
    k_msleep(50);
}

/* ------------------------------------------------------------------ */
/*  TESTING HELPER FUNCTIONS: Thread                                  */
/* ------------------------------------------------------------------ */

K_THREAD_STACK_DEFINE(student_main_stack, STUDENT_MAIN_STACK_SIZE);
struct k_thread student_main_thread;
k_tid_t student_main_tid;
volatile bool main_running = false;

void student_main_entry(void *, void *, void *)
{
    main_running = true;
    student_main();
    main_running = false;
}

/** Kill the background thread cleanly. */
void stop_main(void)
{
    if (main_running) {
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
void start_main(int settle_ms)
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

void simulate_button_click(const struct gpio_dt_spec *button)
{
    gpio_emul_input_set(button->port, button->pin, 1);
    k_sleep(K_MSEC(5));
    gpio_emul_input_set(button->port, button->pin, 0);
}














// #include <zephyr/ztest.h>
// #include <zephyr/kernel.h>
// #include "zephyr_button_test_lib.h"

// /* Thread for running student's main code */
// #define STUDENT_MAIN_STACK_SIZE 1024
// #define STUDENT_MAIN_PRIORITY 5

// K_THREAD_STACK_DEFINE(student_main_stack, STUDENT_MAIN_STACK_SIZE);
// struct k_thread student_main_thread;
// k_tid_t student_main_tid;

// volatile bool main_is_running = false;

// extern int student_main(void);

// /**
//  * @brief Wrapper to run student_main in a thread
//  */
// void student_main_thread_entry(void *p1, void *p2, void *p3)
// {
//   ARG_UNUSED(p1);
//   ARG_UNUSED(p2);
//   ARG_UNUSED(p3);

//   main_is_running = true;
//   student_main();
//   main_is_running = false;
// }

// /**
//  * @brief Test fixture setup
//  */
// void test_before(void *fixture)
// {
//   ARG_UNUSED(fixture);

//   k_event_init(&button_events);
//   k_event_clear(&button_events, BUTTON_EVENT);

//   LED_STATE = LED_OFF;

//   if (main_is_running) {
//     k_thread_abort(student_main_tid);
//     k_msleep(50);
//   }
// }

// /**
//  * @brief Test fixture teardown
//  */
// void test_after(void *fixture)
// {
//   ARG_UNUSED(fixture);

//   if (main_is_running) {
//     k_thread_abort(student_main_tid);
//     k_msleep(50);
//   }
// }