/*****************************************************************/ /**
* @file usb_demo.h
* @brief
* @author bronson.zhan@quectel.com
* @date 2025-04-23
*
* @copyright Copyright (c) 2023 Quectel Wireless Solution, Co., Ltd.
* All Rights Reserved. Quectel Wireless Solution Proprietary and Confidential.
*
* @par EDIT HISTORY FOR MODULE
* <table>
* <tr><th>Date <th>Version <th>Author <th>Description"
* <tr><td>2025-04-23 <td>1.0 <td>Bronson.Zhan <td> Init
* </table>
**********************************************************************/
#ifndef __USB_DEMO_H__
#define __USB_DEMO_H__

#include "qosa_def.h"
#include "qosa_sys.h"

/*===========================================================================
 *  Macro Definition
 ===========================================================================*/

#define CONFIG_QUECOS_USB_DEMO_TASK_STACK_SIZE 4096
#define QUEC_USB_DEMO_TASK_PRIO                QOSA_PRIORITY_NORMAL

/*===========================================================================
  *  Function
  ===========================================================================*/

void quec_usb_demo_init(void);

#endif /* __USB_DEMO_H__ */
