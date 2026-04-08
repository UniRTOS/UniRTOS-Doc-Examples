/*****************************************************************/ /**
* @file gpio_demo.h
* @brief GPIO demonstration header file
* @author larson.li@quectel.com
* @date 2025-05-14
*
* @copyright Copyright (c) 2023 Quectel Wireless Solution, Co., Ltd.
* All Rights Reserved. Quectel Wireless Solution Proprietary and Confidential.
*
* @par EDIT HISTORY FOR MODULE
* <table>
* <tr><th>Date <th>Version <th>Author <th>Description"
* <tr><td>2024-09-02 <td>1.0 <td>Larson.Li <td> Initial version
* </table>
**********************************************************************/
#ifndef __GPIO_DEMO_H__
#define __GPIO_DEMO_H__

#include "qosa_def.h"
#include "qosa_sys.h"

/*===========================================================================
 *  Macro Definition
 ===========================================================================*/

#define CONFIG_QUECOS_GPIO_DEMO_TASK_STACK_SIZE 4096                  // Demo task stack size configuration
#define QUEC_GPIO_DEMO_TASK_PRIO                QOSA_PRIORITY_NORMAL  // Demo task priority configuration

/*===========================================================================
  *  struct
  ===========================================================================*/
/**
 * @enum qosa_gpio_cfg_demo_t
 * @brief GPIO configuration structure definition
 */
typedef struct
{
    qosa_uint8_t          pin_num;
    qosa_gpio_direction_e gpio_dir;
    qosa_gpio_pull_e      gpio_pull;  // for input only
    qosa_gpio_level_e     gpio_lvl;   // for output only
} qosa_gpio_cfg_demo_t;

/*===========================================================================
 *  Function Declaration
 ===========================================================================*/
void quec_gpio_demo_init(void);

#endif /* __GPIO_DEMO_H__ */