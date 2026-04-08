/*****************************************************************/ /**
* @file usbnet_demo.c
* @brief USB Network Tethering Demonstration Program
* @author liaz.liao@quectel.com
* @date 2026-01-07
*
* @copyright Copyright (c) 2023 Quectel Wireless Solution, Co., Ltd.
* All Rights Reserved. Quectel Wireless Solution Proprietary and Confidential.
*
* @par EDIT HISTORY FOR MODULE
* <table>
* <tr><th>Date <th>Version <th>Author <th>Description
* <tr><td>2026-01-07 <td>1.0 <td>liaz.liao <td> Initial version
* </table>
**********************************************************************/
#include "qosa_sys.h"
#include "qosa_def.h"
#include "qosa_log.h"
#include "qosa_usbnet.h"
#include "qosa_datacall.h"
#include "qosa_power.h"

#define QOS_LOG_TAG                       LOG_TAG_DEMO

/** Maximum wait time for network attachment (seconds) */
#define USBNET_DEMO_WAIT_ATTACH_MAX_WAIT_TIME   30


/** @brief Usbnet demo task handle */
qosa_task_t g_usbnet_demo_task_ptr = QOSA_NULL;


/**
 * @brief Callback function for USB network operation completion.
 *
 * @param[in] ctx User context pointer, expected to be `qosa_usbnet_config_t*`.
 * @param[in] argv Operation confirmation data, expected to be `qosa_usbnet_general_cnf_t*`.
 *
 * @return None
 */
static void _usbnet_demo_func_cb(void *ctx, void *argv)
{
    qosa_usbnet_config_t      *usbnet_config = (qosa_usbnet_config_t *)ctx;
    qosa_usbnet_general_cnf_t *cnf = argv;

    QLOGI("simid:%d, method:%d, pdpid:%d",usbnet_config->simid, usbnet_config->method, usbnet_config->pdpid);

    if (QOSA_OK == cnf->err_code)
    {
        QLOGI("Usbnet Successed");
    }
    else
    {
        QLOGI("Usbnet Failed");
    }
    
    qosa_free(usbnet_config);

    return ;
}

/**
 * @brief Main task function for the USB network tethering demonstration.
 *
 * This function demonstrates the complete workflow:
 * 1. Configuring the USB network mode (RNDIS/ECM).
 * 2. Waiting for network registration.
 * 3. Setting up a PDP context and establishing a data call.
 * 4. Starting and stopping USB network tethering.
 *
 * @param[in] arg Task argument (unused).
 * @return None
 */
void qosa_usbnet_demo_task(void *arg)
{
    QOSA_UNUSED(arg);

    int ret = 0;
    qosa_usbnet_type_e usbnet_type = QOSA_USBNET_TYPE_RNDIS;
    qosa_usbnet_type_e curr_usbnet_type = QOSA_USBNET_TYPE_ECM;
    qosa_uint8_t simid = 0;
    qosa_uint8_t profile_idx = 1;
    qosa_usbnet_method_e method = QOSA_USBNET_METHOD_AUTO_CONNECT;
    qosa_pdp_context_t      pdp_ctx = {0};
    qosa_datacall_conn_t conn = {0};
    qosa_usbnet_config_t *config = QOSA_NULL;
    
    config = (qosa_usbnet_config_t *)qosa_malloc(sizeof(qosa_usbnet_config_t));
    if (config == QOSA_NULL)
    {
        QLOGI("Malloc fail");
        return;
    }

    // Step 1: Set USB network mode.
    // Read the current USB network mode.
    // If the mode to set differs from the current configuration, set it and then reboot.
    // If the mode to set is the same as the current configuration, proceed to the next step: check module network registration.
    qosa_usbnet_get_type(simid, &curr_usbnet_type);

    if(usbnet_type != curr_usbnet_type)
    {
        qosa_task_sleep_sec(20);
        if (QOSA_USBNET_ERR_OK != qosa_usbnet_set_type(simid, usbnet_type))
        {
            goto exit;
        }
        QLOGI("power reset");
        qosa_task_sleep_sec(20);
        qosa_power_reset(QOSA_RESET_NORMAL);
        goto exit;
    }

    // Step 2: Module network registration.
    if (!qosa_datacall_wait_attached(simid, USBNET_DEMO_WAIT_ATTACH_MAX_WAIT_TIME))
    {
        QLOGI("attach fail");
        goto exit;
    }

    // Step 3: Configure PDP context: APN, IP type.
    // If the operator has restrictions on the APN during registration, needs to be set the APN provided by the operator
    const char *apn_str = "test";
    pdp_ctx.apn_valid = QOSA_TRUE;
    pdp_ctx.pdp_type = QOSA_PDP_TYPE_IP;  // ipv4
    if (pdp_ctx.apn_valid)
    {
        qosa_memcpy(pdp_ctx.apn, apn_str, qosa_strlen(apn_str));
    }

    ret = qosa_datacall_set_pdp_context(simid, profile_idx, &pdp_ctx);
    QLOGI("set pdp context, ret=%d", ret);

    // Step 4: Module establishes a data call.
    conn = qosa_datacall_conn_new(simid, profile_idx, QOSA_DATACALL_CONN_UNDEFINED);
    if (!qosa_datacall_get_status(conn))
    {
        ret = qosa_datacall_start(conn, USBNET_DEMO_WAIT_ATTACH_MAX_WAIT_TIME);
        if (ret != QOSA_DATACALL_OK)
        {
            QLOGI("datacall fail ,ret=%d", ret);
            goto exit;
        }
    }

    // Step 5: Start network sharing via USB (qosa_usbnet_start).
    // Note: Profile ID (pdpid) is allowed to be 0 only when the method is 0.
    if ((profile_idx == 0) && (method != 0))
    {
        QLOGI("usbnet params fail");
        goto exit;
    }

    config->simid = simid;
    config->method = method;
    config->pdpid = profile_idx;
    if (0 != qosa_usbnet_set_config(simid, config))
    {
        QLOGI("usbnet set config fail ");
        goto exit;
    }

    if (config->method != QOSA_USBNET_METHOD_DISABLE)
    {
        qosa_usbnet_start(simid, _usbnet_demo_func_cb, config);
    }
    else
    {
        qosa_usbnet_stop(simid, _usbnet_demo_func_cb, config);
    }

    qosa_task_sleep_sec(20);

    // Step 6: Stop network sharing, end the process (qosa_usbnet_stop).
    if (config->method != QOSA_USBNET_METHOD_DISABLE)
    {
        qosa_usbnet_stop(simid, _usbnet_demo_func_cb, config);
    }

exit:

    if (g_usbnet_demo_task_ptr != QOSA_NULL)
    {
        qosa_task_delete(g_usbnet_demo_task_ptr);
        g_usbnet_demo_task_ptr = QOSA_NULL;
    }

    qosa_free(config);

    return;
}

/**
 * @brief Initialize the USB network tethering demonstration program.
 *
 * This function sets up the USB network configuration, event callbacks,
 * and creates the necessary tasks for running the demonstration.
 *
 * @return void
 */
void unir_usbnet_demo_init(void)
{
    int err = 0;

    QLOGI("enter UniRTOS USBNET DEMO !!!");
    if (g_usbnet_demo_task_ptr == QOSA_NULL)
    {
        err = qosa_task_create(&g_usbnet_demo_task_ptr, 4 * 1024, QOSA_PRIORITY_NORMAL, "usbnet_demo", qosa_usbnet_demo_task, QOSA_NULL);
        if (err != QOSA_OK)
        {
            QLOGI("USBNET demo init task create error");
            return;
        }
    }

    return;
}