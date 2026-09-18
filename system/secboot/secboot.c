/*****************************************************************/ /**
* @file secboot_demo.c
* @brief
* @author felix.liu@quectel.com
* @date 2026-01-20
*
* @copyright Copyright (c) 2023 Quectel Wireless Solution, Co., Ltd.
* All Rights Reserved. Quectel Wireless Solution Proprietary and Confidential.
*
* @par EDIT HISTORY FOR MODULE
* <table>
* <tr><th>Date <th>Version <th>Author <th>Description"
* <tr><td>2026-01-20 <td>1.0 <td>Felix.Liu <td> Init
* </table>
**********************************************************************/

#include "qosa_sys.h"
#include "qosa_secboot.h"
#include "qosa_def.h"
#include "qosa_log.h"

/*===========================================================================
 *  Macro Definition
 ===========================================================================*/
#define QOS_LOG_TAG LOG_TAG_DEMO

/*===========================================================================
 *  Variate
 ===========================================================================*/
static qosa_task_t g_unir_secboot_task = QOSA_NULL;


static void unir_secboot_demo_process(void *ctx)
{
    qosa_secboot_error_e ret = QOSA_SECBOOT_OK;   
    qosa_uint8_t status = 0;

    // Delay 5 seconds to prevent log loss
    qosa_task_sleep_ms(5000);

    // enable secboot function
    ret = qosa_secboot_enable();
    if(ret != QOSA_SECBOOT_OK)
    {
        QLOGD("SECBOOT ENABLE FAILED[%d]", ret);
    }

    // Get of secboot enable status
    while (1)
    {
        ret = qosa_secboot_get_enable_state(&status);
        if(ret != QOSA_SECBOOT_OK)
        {
            QLOGD("SECBOOT GET status FAILED[%d]", ret);
        }

        QLOGD("secboot enable state=%d",status);
        qosa_task_sleep_ms(4000);
    }
}

void unir_secboot_demo_init(void)
{
    int err = 0;

    err = qosa_task_create(&g_unir_secboot_task, 4096, QOSA_PRIORITY_NORMAL, "secboot_demo", unir_secboot_demo_process, QOSA_NULL);
    if (err != QOSA_OK)
    {
        QLOGE("secboot task create error");
        return;
    }
}