/*****************************************************************/ /**
* @file adc_demo.c
* @brief
* @author bronson.zhan@quectel.com
* @date 2025-04-28
*
* @copyright Copyright (c) 2023 Quectel Wireless Solution, Co., Ltd.
* All Rights Reserved. Quectel Wireless Solution Proprietary and Confidential.
*
* @par EDIT HISTORY FOR MODULE
* <table>
* <tr><th>Date <th>Version <th>Author <th>Description
* <tr><td>2025-04-28 <td>1.0 <td>Bronson.Zhan <td> Init
* </table>
**********************************************************************/
#include "qosa_sys.h"
#include "qosa_adc.h"
#include "qosa_def.h"
#include "qosa_log.h"
#include "adc_demo.h"

/*===========================================================================
 *  Macro Definition
 ===========================================================================*/
#define QOS_LOG_TAG   LOG_TAG_DEMO

/*===========================================================================
 *  Variate
 ===========================================================================*/
static qosa_task_t g_quec_adc_demo_task = QOSA_NULL;

/*===========================================================================
 *  Static API Functions
 ===========================================================================*/
/**
 * @brief ADC processing function
 *
 * This function continuously reads voltage values from ADC0 and ADC1 channels
 * and performs measurements at different scale ranges.
 * The function iterates through all available ADC scale ranges, sets the scale,
 * reads the voltage value, and prints logs.
 *
 * @note Note: Not all platforms support scale setting
 */
 static void quec_adc_demo_process(void *ctx)
{
    int                  adc_val = 0;
    qosa_adc_aux_scale_e adc_scale = QOSA_ADC_SCALE_LEVEL_0;

    while (1)
    {
        // Iterate through all ADC scale levels
        for (adc_scale = QOSA_ADC_SCALE_LEVEL_0; adc_scale < QOSA_ADC_SCALE_MAX; adc_scale++)
        {
            // Set scale for ADC0 and ADC1 channels
            qosa_adc_ioctl(QOSA_ADC0_CHANNEL, QOSA_ADC_IOCTL_SET_SCALE, (void *)&adc_scale);
            qosa_adc_ioctl(QOSA_ADC1_CHANNEL, QOSA_ADC_IOCTL_SET_SCALE, (void *)&adc_scale);
            
            // Read ADC0 channel voltage value and print
            qosa_adc_get_volt(QOSA_ADC0_CHANNEL, &adc_val);
            QLOGD("QOSA_ADC0_CHANNEL val: %d\r\n", adc_val);

            // Read ADC1 channel voltage value and print
            qosa_adc_get_volt(QOSA_ADC1_CHANNEL, &adc_val);
            QLOGD("QOSA_ADC1_CHANNEL val: %d\r\n", adc_val);

            // Delay 2 seconds before next measurement
            qosa_task_sleep_ms(2000);
        }
    }
}

/*===========================================================================
 *  Public API Functions
 ===========================================================================*/

void quec_adc_demo_init(void)
{
    QLOGV("enter Quectel ADC DEMO !!!");
    if (g_quec_adc_demo_task == QOSA_NULL)
    {
        qosa_task_create(
            &g_quec_adc_demo_task,
            CONFIG_QUECOS_ADC_DEMO_TASK_STACK_SIZE,
            QUEC_ADC_DEMO_TASK_PRIO,
            "adc_demo",
            quec_adc_demo_process,
            QOSA_NULL,
            1
        );
    }
}