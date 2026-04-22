#ifndef LED_TEST_H
#define LED_TEST_H

#include "bme554_lib.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

/* ------------------------------------------------------------------ */
/*  Thread config for running student_main in background              */
/* ------------------------------------------------------------------ */
#define STUDENT_MAIN_STACK_SIZE 2048
#define STUDENT_MAIN_PRIORITY   5

/* ------------------------------------------------------------------ */
/*  Student code                                                      */
/* ------------------------------------------------------------------ */
extern static const struct gpio_dt_spec blinker;
extern int student_main(void);  /* renamed by CMake */



#endif // LED_TEST_H