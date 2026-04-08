/*****************************************************************/ /**
* @file pwm_demo.c
* @brief
* @author larson.li@quectel.com
* @date 2025-05-14
*
* @copyright Copyright (c) 2023 Quectel Wireless Solution, Co., Ltd.
* All Rights Reserved. Quectel Wireless Solution Proprietary and Confidential.
*
* @par EDIT HISTORY FOR MODULE
* <table>
* <tr><th>Date <th>Version <th>Author <th>Description"
* <tr><td>2023-06-13 <td>1.0 <td>Larson.Li <td> Init
* </table>
**********************************************************************/
#include "qosa_sys.h"
#include "qosa_gpio.h"
#include "qosa_pwm.h"
#include "qosa_def.h"
#include "qosa_log.h"
#include "pwm_demo.h"
#include <stdlib.h>
#include <string.h>

/*===========================================================================
 *  Macro Definition
 ===========================================================================*/
#define QOS_LOG_TAG   LOG_TAG_DEMO
#define QUEC_PWM_TSET_PIN 21
/*===========================================================================
 *  Variate
 ===========================================================================*/
static qosa_task_t g_quec_pwm_demo_task = QOSA_NULL;

/**
 * @brief PWM function test function, using 26MHz clock source for PWM output test
 *
 * This function configures PWM parameters to output PWM waveforms with different duty cycles
 * on the specified pin, used to verify if the PWM function is working properly.
 *
 * Test process:
 * 1. Configure pin 21 for PWM function
 * 2. Set PWM parameters: 26MHz clock source, period 2000, prescaler coefficient 1
 * 3. Output 50% duty cycle PWM waveform for 10 seconds
 * 4. Adjust to 25% duty cycle PWM waveform for 10 seconds
 * 5. Turn off PWM output
  *
 * @param None
 * @return None
 */
void quec_demo_26M_test(void)
{
    qosa_pwm_info_t pwm_info;

    // Use pin 21 for PWM0 test
    qosa_pin_set_func(QUEC_PWM_TSET_PIN, 5);

    /* Configure PWM parameters: 26MHz clock source, period 2000, high level 1000, prescaler coefficient 1 */
    pwm_info.clk_src = QOSA_FCLK_SEL_26M;
    pwm_info.high_one_cycle_duration = 1000;
    pwm_info.total_one_cycle_duration = 2000;
    pwm_info.pwm_psc = 1;
    qosa_pwm_config(0, &pwm_info);

    /* Start PWM output, 50% duty cycle, for 10 seconds */
    qosa_pwm_enable(0, 1000);
    qosa_task_sleep_sec(10);

    /* Adjust PWM duty cycle to 25%, for 10 seconds */
    qosa_pwm_enable(0, 500);
    qosa_task_sleep_sec(10);

    /* Turn off PWM output */
    qosa_pwm_disable(0);
}
/**
 * @brief PWM function test function
 *
 * This function is used to test APWM0 function, by configuring PIN 21 to output PWM signal,
 * and running at different duty cycles, finally turning off PWM output.
 *
 */
void quec_demo_slp_test(void)
{
    qosa_pwm_info_t pwm_info;
    qosa_pin_cfg_t  pin_cfg;

    // Use pin 21 for APWM0 test   APW is actually AGPIO using timer simulation, not hardware PWM, accuracy is poor, so need to configure as GPIO
    qosa_pin_set_func(QUEC_PWM_TSET_PIN, 0);
    qosa_get_pin_default_cfg(QUEC_PWM_TSET_PIN, &pin_cfg);
    qosa_gpio_init(pin_cfg.gpio_num, QOSA_GPIO_DIRECTION_OUTPUT, QOSA_GPIO_PULL_DOWN, QOSA_GPIO_LEVEL_LOW);
    
    /* Configure PWM parameters: 32K clock source, high level 1000 cycles, total period 2000 cycles, prescaler 1 */
    pwm_info.clk_src = QOSA_FCLK_SEL_32K;
    pwm_info.high_one_cycle_duration = 1000;
    pwm_info.total_one_cycle_duration = 2000;
    pwm_info.pwm_psc = 1;
    qosa_pwm_config(0, &pwm_info);

    /* Start PWM output and test different duty cycles */
    qosa_pwm_enable(0, 1000);  //50% duty cycle
    qosa_task_sleep_sec(10);
    qosa_pwm_enable(0, 500);   //25% duty cycle
    qosa_task_sleep_sec(10);

    /* Turn off PWM output */
    qosa_pwm_disable(0);
}
static void quec_pwm_demo_process(void *ctx)
{
    qosa_task_sleep_sec(10);
    // Use 26M clock during normal operation
    quec_demo_26M_test();

    // Maintain PWM output during sleep with 32k clock
    quec_demo_slp_test();
}

/*===========================================================================
 *  Public API Functions
 ===========================================================================*/
void quec_pwm_demo_init(void)
{
    QLOGV("enter Quectel PWM DEMO !!!");
    if (g_quec_pwm_demo_task == QOSA_NULL)
    {
        qosa_task_create(
            &g_quec_pwm_demo_task,
            CONFIG_QUECOS_PWM_DEMO_TASK_STACK_SIZE,
            QUEC_PWM_DEMO_TASK_PRIO,
            "pwm_demo",
            quec_pwm_demo_process,
            QOSA_NULL,
            1
        );
    }
}
