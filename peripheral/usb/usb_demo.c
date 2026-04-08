/*****************************************************************/ /**
* @file usb_demo.c
* @brief
* @author bronson.zhan@quectel.com
* @date 2025-04-23
*
* @copyright Copyright (c) 2023 Quectel Wireless Solution, Co., Ltd.
* All Rights Reserved. Quectel Wireless Solution Proprietary and Confidential.
*
* @par EDIT HISTORY FOR MODULE
* <table>
* <tr><th>Date <th>Version <th>Author <th>Description
* <tr><td>2025-04-23 <td>1.0 <td>Bronson.Zhan <td> Init
* </table>
**********************************************************************/
#include "qosa_sys.h"
#include "qosa_usb.h"
#include "qosa_def.h"
#include "qosa_log.h"

#include "usb_demo.h"

/*===========================================================================
 *  Macro Definition
 ===========================================================================*/

#define QOS_LOG_TAG LOG_TAG_USB_API

/*===========================================================================
 *  Variate
 ===========================================================================*/

static qosa_task_t g_quec_usb_demo_task = QOSA_NULL;
/*===========================================================================
 *  Static API Functions
 ===========================================================================*/
/**
 * @brief Callback function triggered by VBUS interruption
 */
static qosa_uint32_t quec_vbus_callback(qosa_usb_vbus_state_e state, void *ctx)
{
    QLOGI("VBUS state changed(%d)", state);
    return 0;
}

/**
 * @brief USB demo task, used for configuring and testing USB interface functions
 */
 /*===========================================================================
 *  Public API Functions
 ===========================================================================*/
void quec_usb_demo_init(void)
{
    QLOGI("enter Quectel USB DEMO !!!");
    if (g_quec_usb_demo_task == QOSA_NULL)
    {
        qosa_task_create(
            &g_quec_usb_demo_task,
            CONFIG_QUECOS_USB_DEMO_TASK_STACK_SIZE,
            QUEC_USB_DEMO_TASK_PRIO,
            "usb_demo",
            quec_usb_demo_process,
            QOSA_NULL,
            1
        );
    }
}
