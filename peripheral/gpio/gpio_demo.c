/*****************************************************************/ /**
* @file gpio_demo.c
* @brief GPIO demonstration application
* @author larson.li@quectel.com
* @date 2025-05-14
*
* @copyright Copyright (c) 2023 Quectel Wireless Solution, Co., Ltd.
* All Rights Reserved. Quectel Wireless Solution Proprietary and Confidential.
*
* @par EDIT HISTORY FOR MODULE
* <table>
* <tr><th>Date <th>Version <th>Author <th>Description"
* <tr><td>2025-05-14 <td>1.0 <td>Larson.Li <td> Initial version
* </table>
**********************************************************************/
#include "qosa_sys.h"
#include "qosa_gpio.h"
#include "qosa_def.h"
#include "qosa_log.h"
#include "gpio_demo.h"
#include <stdlib.h>
#include <string.h>

/*===========================================================================
 *  Macro Definition
 ===========================================================================*/
#define QOS_LOG_TAG   LOG_TAG_DEMO

/*===========================================================================
 *  Variate
 ===========================================================================*/
static qosa_task_t        g_quec_gpio_demo_task = QOSA_NULL;
static qosa_gpio_cfg_demo_t g_quec_gpio_cfg[] = {
    /* pin_num   gpio_dir     gpio_pull    gpio_lvl    */
    {21, QOSA_GPIO_DIRECTION_INPUT, QOSA_GPIO_PULL_DOWN, QOSA_GPIO_LEVEL_LOW},  // Configure as input with pull-down
    {52, QOSA_GPIO_DIRECTION_OUTPUT, QOSA_GPIO_PULL_UP, QOSA_GPIO_LEVEL_HIGH}   // Configure as output with high level
};

/*===========================================================================
 *  Static API Functions
 ===========================================================================*/
/**
 * @brief Initialize GPIO pin configuration
 *
 * This function iterates through the global GPIO configuration array and initializes
 * each GPIO pin with the specified configuration, including setting pin function,
 * direction, pull-up/pull-down resistors, and initial level state.
 *
 * @param None
 *
 * @return None
 *
 * @note This function depends on the global configuration array g_quec_gpio_cfg
 */
void quec_gpio_demo_inits(void)
{
    qosa_uint8_t   num;
    qosa_pin_cfg_t pin_cfg;

    // Iterate through all GPIO configuration items and initialize each pin
    for (num = 0; num < sizeof(g_quec_gpio_cfg) / sizeof(g_quec_gpio_cfg[0]); num++)
    {
        qosa_memset(&pin_cfg, 0, sizeof(qosa_pin_cfg_t));
        // Get default pin configuration
        qosa_get_pin_default_cfg(g_quec_gpio_cfg[num].pin_num, &pin_cfg);
        // Set pin function to GPIO mode
        qosa_pin_set_func(g_quec_gpio_cfg[num].pin_num, pin_cfg.gpio_func);
        // Initialize GPIO parameters including direction, pull-up/down, and level state
        qosa_gpio_init(pin_cfg.gpio_num, g_quec_gpio_cfg[num].gpio_dir, g_quec_gpio_cfg[num].gpio_pull, g_quec_gpio_cfg[num].gpio_lvl);
        // Print corresponding configuration information
        QLOGD(
            "set info gpio_num:%d, gpio_dir:%d, gpio_pull:%d, gpio_lvl:%d",
            pin_cfg.gpio_num,
            g_quec_gpio_cfg[num].gpio_dir,
            g_quec_gpio_cfg[num].gpio_pull,
            g_quec_gpio_cfg[num].gpio_lvl
        );
    }
}

/**
 * @brief Get and print information for all configured GPIO pins
 *
 * This function iterates through all configured GPIO pins, retrieves each pin's
 * direction, pull-up/pull-down configuration, and current level state, then
 * prints this information to verify if it matches expectations.
 *
 * @param None
 *
 * @return None
 */
void quec_gpio_demo_get_info(void)
{
    qosa_uint8_t          num;
    qosa_pin_cfg_t        pin_cfg;
    qosa_gpio_direction_e gpio_dir;
    qosa_gpio_pull_e      gpio_pull;
    qosa_gpio_level_e     gpio_lvl;

    // Iterate through all configured GPIO pins
    for (num = 0; num < sizeof(g_quec_gpio_cfg) / sizeof(g_quec_gpio_cfg[0]); num++)
    {
        // Initialize pin configuration structure
        qosa_memset(&pin_cfg, 0, sizeof(qosa_pin_cfg_t));
        // Get default pin configuration
        qosa_get_pin_default_cfg(g_quec_gpio_cfg[num].pin_num, &pin_cfg);
        // Get GPIO direction, pull-up/down configuration, and level state
        qosa_gpio_get_direction(pin_cfg.gpio_num, &gpio_dir);
        qosa_gpio_get_pull(pin_cfg.gpio_num, &gpio_pull);
        qosa_gpio_get_level(pin_cfg.gpio_num, &gpio_lvl);
        // Print GPIO pin information
        QLOGD("get info gpio_num:%d, gpio_dir:%d, gpio_pull:%d, gpio_lvl:%d", pin_cfg.gpio_num, gpio_dir, gpio_pull, gpio_lvl);
    }
}

/**
 * @brief GPIO output function test
 *
 * This function tests the output functionality of all configured GPIO pins.
 * The test is divided into two phases:
 * Phase 1: Set all GPIOs to output mode and output low level;
 * Phase 2: Set all GPIO outputs to high level.
 * A 3-second delay is added between each phase to facilitate observation of GPIO state changes.
 *
 */
void quec_gpio_demo_set_out_test(void)
{
    qosa_uint8_t          num;
    qosa_pin_cfg_t        pin_cfg;
    qosa_gpio_direction_e gpio_dir = QOSA_GPIO_DIRECTION_OUTPUT;
    qosa_gpio_level_e     gpio_lvl = QOSA_GPIO_LEVEL_LOW;
    
    // Phase 1: Set all GPIOs to output mode and output low level
    for (num = 0; num < sizeof(g_quec_gpio_cfg) / sizeof(g_quec_gpio_cfg[0]); num++)
    {
        gpio_lvl = QOSA_GPIO_LEVEL_LOW;
        gpio_dir = QOSA_GPIO_DIRECTION_OUTPUT;
        qosa_memset(&pin_cfg, 0, sizeof(qosa_pin_cfg_t));
        qosa_get_pin_default_cfg(g_quec_gpio_cfg[num].pin_num, &pin_cfg);
        qosa_gpio_set_direction(pin_cfg.gpio_num, gpio_dir);
        qosa_gpio_set_level(pin_cfg.gpio_num, gpio_lvl);
        QLOGD("set gpio_num:%d, gpio_dir:%d, gpio_lvl:%d", pin_cfg.gpio_num, gpio_dir, gpio_lvl);

        qosa_gpio_get_direction(pin_cfg.gpio_num, &gpio_dir);
        qosa_gpio_get_level(pin_cfg.gpio_num, &gpio_lvl);
        QLOGD("get gpio_num:%d, gpio_dir:%d, gpio_lvl:%d", pin_cfg.gpio_num, gpio_dir, gpio_lvl);
    }
    // Delay 3 seconds to observe low level state
    qosa_task_sleep_sec(3);
    // Phase 2: Set all GPIO outputs to high level
    for (num = 0; num < sizeof(g_quec_gpio_cfg) / sizeof(g_quec_gpio_cfg[0]); num++)
    {
        gpio_lvl = QOSA_GPIO_LEVEL_HIGH;
        qosa_memset(&pin_cfg, 0, sizeof(qosa_pin_cfg_t));
        qosa_get_pin_default_cfg(g_quec_gpio_cfg[num].pin_num, &pin_cfg);
        qosa_gpio_set_level(pin_cfg.gpio_num, gpio_lvl);
        QLOGD("set gpio_num:%d, gpio_lvl:%d", pin_cfg.gpio_num, gpio_lvl);

        qosa_gpio_get_level(pin_cfg.gpio_num, &gpio_lvl);
        QLOGD("get gpio_num:%d, gpio_lvl:%d", pin_cfg.gpio_num, gpio_lvl);
    }
    qosa_task_sleep_sec(3);
}

/**
 * @brief GPIO callback function
 *
 * This function serves as a callback handler for GPIO interrupts or events.
 * It is called when the corresponding GPIO event is triggered.
 * The function internally prints log information and can perform simple
 * processing in the callback based on actual requirements.
 *
 * @param ctx Context parameter for the callback function, used to pass
 *            callback-related context information
 * @return No return value
 */
void quec_gpio_cb01(void *ctx)
{
    QLOGD("quec_gpio_cb01");
}

/**
 * @brief GPIO callback function
 *
 * This function serves as a callback handler for GPIO interrupts or events.
 * It is called when the corresponding GPIO event is triggered.
 * The function internally prints log information and can perform simple
 * processing in the callback based on actual requirements.
 *
 * @param ctx Context parameter for the callback function, used to pass
 *            callback-related context information
 * @return No return value
 */
 void quec_gpio_cb02(void *ctx)
{
    QLOGD("quec_gpio_cb02");
}

/**
 * @brief GPIO input function test for testing GPIO input mode configuration,
 *        pull-up/pull-down resistor switching, and interrupt registration functionality.
 *
 * This function primarily performs the following operations:
 * 1. Iterate through all predefined GPIO pin configuration items, set them to input mode
 *    and configure as pull-down resistors;
 * 2. Read and print current GPIO direction and pull-up/down status to verify configuration success;
 * 3. Wait 3 seconds, then reconfigure all GPIOs to pull-up mode and read/print status again;
 * 4. Register interrupt handlers for two specific GPIOs, configured for rising edge
 *    and falling edge triggering respectively.
 *
 */
 void quec_gpio_demo_set_in_test(void)
{
    qosa_uint8_t          num;
    qosa_pin_cfg_t        pin_cfg;
    qosa_gpio_direction_e gpio_dir = QOSA_GPIO_DIRECTION_INPUT;
    qosa_gpio_pull_e      gpio_pull = QOSA_GPIO_PULL_DOWN;
    qosa_int_cfg_t        quec_int_cfg = {0};

    // Phase 1: Iterate through all GPIO configuration items, set to input mode,
    // configure as pull-down resistors, read configuration, and print information
    for (num = 0; num < sizeof(g_quec_gpio_cfg) / sizeof(g_quec_gpio_cfg[0]); num++)
    {
        gpio_pull = QOSA_GPIO_PULL_DOWN;
        qosa_memset(&pin_cfg, 0, sizeof(qosa_pin_cfg_t));
        qosa_get_pin_default_cfg(g_quec_gpio_cfg[num].pin_num, &pin_cfg);
        qosa_gpio_set_direction(pin_cfg.gpio_num, gpio_dir);
        qosa_gpio_set_pull(pin_cfg.gpio_num, gpio_pull);
        QLOGD("set gpio_num:%d, gpio_dir:%d, gpio_pull:%d", pin_cfg.gpio_num, gpio_dir, gpio_pull);

        qosa_gpio_get_direction(pin_cfg.gpio_num, &gpio_dir);
        qosa_gpio_get_pull(pin_cfg.gpio_num, &gpio_pull);
        QLOGD("get gpio_num:%d, gpio_dir:%d, gpio_pull:%d", pin_cfg.gpio_num, gpio_dir, gpio_pull);
    }
    qosa_task_sleep_sec(3);
    
    // Phase 2: Reconfigure GPIOs to pull-up mode, read configuration, and print information
    for (num = 0; num < sizeof(g_quec_gpio_cfg) / sizeof(g_quec_gpio_cfg[0]); num++)
    {
        gpio_pull = QOSA_GPIO_PULL_UP;
        qosa_memset(&pin_cfg, 0, sizeof(qosa_pin_cfg_t));
        qosa_get_pin_default_cfg(g_quec_gpio_cfg[num].pin_num, &pin_cfg);
        qosa_gpio_set_pull(pin_cfg.gpio_num, gpio_pull);
        QLOGD("set gpio_num:%d, gpio_pull:%d", pin_cfg.gpio_num, gpio_pull);

        qosa_gpio_get_pull(pin_cfg.gpio_num, &gpio_pull);
        QLOGD("get gpio_num:%d, gpio_pull:%d", pin_cfg.gpio_num, gpio_pull);
    }
    qosa_task_sleep_sec(3);

    // Phase 3: Register GPIO interrupt handlers
    // Register interrupt for pin 21: input with pull-down, rising edge trigger
    // Register interrupt for pin 52: input with pull-up, falling edge trigger
    qosa_memset(&pin_cfg, 0, sizeof(qosa_pin_cfg_t));
    qosa_get_pin_default_cfg(g_quec_gpio_cfg[0].pin_num, &pin_cfg);
    quec_int_cfg.gpio_num = pin_cfg.gpio_num;
    quec_int_cfg.gpio_pull = QOSA_GPIO_PULL_DOWN;
    quec_int_cfg.gpio_debounce = 0;
    quec_int_cfg.interrupt_cb = quec_gpio_cb01;
    qosa_interrupt_register(&quec_int_cfg);
    qosa_interrupt_enable(quec_int_cfg.gpio_num, QOSA_GPIO_TRIGGER_RISING_EDGE);
    
    qosa_memset(&quec_int_cfg, 0, sizeof(qosa_int_cfg_t));
    qosa_memset(&pin_cfg, 0, sizeof(qosa_pin_cfg_t));
    qosa_get_pin_default_cfg(g_quec_gpio_cfg[1].pin_num, &pin_cfg);
    quec_int_cfg.gpio_num = pin_cfg.gpio_num;
    quec_int_cfg.gpio_pull = QOSA_GPIO_PULL_UP;
    quec_int_cfg.gpio_debounce = 0;
    quec_int_cfg.interrupt_cb = quec_gpio_cb02;
    qosa_interrupt_register(&quec_int_cfg);
    qosa_interrupt_enable(quec_int_cfg.gpio_num, QOSA_GPIO_TRIGGER_FALLING_EDGE);
}

static void quec_gpio_demo_process(void *ctx)
{
    qosa_task_sleep_sec(10);
    // 1. Initialize corresponding pins with required configuration
    quec_gpio_demo_inits();

    // 2. Get pin configuration information to check if it meets expectations
    quec_gpio_demo_get_info();

    // 3. Output: high/low level testing
    quec_gpio_demo_set_out_test();

    // 4. Input: pull-up/pull-down + interrupt testing
    quec_gpio_demo_set_in_test();
}

void quec_gpio_demo_init(void)
{
    QLOGV("enter Quectel GPIO DEMO !!!");
    if (g_quec_gpio_demo_task == QOSA_NULL)
    {
        qosa_task_create(
            &g_quec_gpio_demo_task,
            CONFIG_QUECOS_GPIO_DEMO_TASK_STACK_SIZE,
            QUEC_GPIO_DEMO_TASK_PRIO,
            "gpio_demo",
            quec_gpio_demo_process,
            QOSA_NULL,
            1
        );
    }
}

