/*****************************************************************/ /**
* @file pwm_demo.h
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
* <tr><td>2024-09-02 <td>1.0 <td>Larson.Li <td> Init
* </table>
**********************************************************************/
#ifndef __PWM_DEMO_H__
#define __PWM_DEMO_H__

#include "qosa_def.h"
#include "qosa_sys.h"

/*===========================================================================
 *  Macro Definition
 ===========================================================================*/
#define CONFIG_QUECOS_PWM_DEMO_TASK_STACK_SIZE 4096                  // Demo task stack size configuration
#define QUEC_PWM_DEMO_TASK_PRIO                QOSA_PRIORITY_NORMAL  // Demo task priority configuration

/*===========================================================================
 *  Function Declaration
 ===========================================================================*/
void quec_pwm_demo_init(void);

#endif /* __PWM_DEMO_H__ */
