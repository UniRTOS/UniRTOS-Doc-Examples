/*****************************************************************/ /**
* @file adc_demo.h
* @brief
* @author bronson.zhan@quectel.com
* @date 2025-04-28
*
* @copyright Copyright (c) 2023 Quectel Wireless Solution, Co., Ltd.
* All Rights Reserved. Quectel Wireless Solution Proprietary and Confidential.
*
* @par EDIT HISTORY FOR MODULE
* <table>
* <tr><th>Date <th>Version <th>Author <th>Description"
* <tr><td>2025-04-28 <td>1.0 <td>Bronson.Zhan <td> Init
* </table>
**********************************************************************/
#ifndef __ADC_DEMO_H__
#define __ADC_DEMO_H__

#include "qosa_def.h"
#include "qosa_sys.h"

/*===========================================================================
 *  Macro Definition
 ===========================================================================*/

#define CONFIG_QUECOS_ADC_DEMO_TASK_STACK_SIZE 4096                  // 4K stack size configuration for ADC demo task
#define QUEC_ADC_DEMO_TASK_PRIO                QOSA_PRIORITY_NORMAL  // ADC demo task priority configuration

/*===========================================================================
 *  Function Declaration
 ===========================================================================*/
void quec_adc_demo_init(void);

#endif /* __ADC_DEMO_H__ */