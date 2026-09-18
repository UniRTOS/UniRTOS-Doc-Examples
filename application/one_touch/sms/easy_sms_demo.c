/*****************************************************************/ /**
* @file easy_sms_demo.c
* @brief
* @author frankie.liao@quectel.com
* @date 2025-11-20
*
* @copyright Copyright (c) 2023 Quectel Wireless Solution, Co., Ltd.
* All Rights Reserved. Quectel Wireless Solution Proprietary and Confidential.
*
* @par EDIT HISTORY FOR MODULE
* <table>
* <tr><th>Date <th>Version <th>Author <th>Description
* <tr><td>2025-11-20 <td>1.0 <td>Frankie.liao <td> Init
* </table>
**********************************************************************/
#include "qosa_log.h"
#include "qosa_sms.h"
#include "qosa_sim.h"
#include "easy_sms.h"


/*===========================================================================
 * Macro Definition
 ===========================================================================*/
#define QOS_LOG_TAG LOG_TAG_DEMO

/*===========================================================================
 *  Variable definitions
 ===========================================================================*/
/** Easy SMS demonstration task pointer */
static qosa_task_t g_easy_sms_demo_task_ptr = QOSA_NULL;
/** Easy SMS Initialization semaphore */
static qosa_sem_t g_easy_sms_init_semaphore = QOSA_NULL;

/**
 * @brief Easy SMS uers event callback.
 *
 * @param[in] simid
 *          - SIM ID
 * @param[in] event_id
 *          - event ID
 * @param[in] ctx
 *          - context
 * @return void
 */
void user_sms_event_callback(qosa_uint8_t simid, qosa_uint8_t event_id, void *ctx)
{
    switch (event_id)
    {
        case QOSA_EASY_SMS_INIT_OK: {
            QLOGI("[%d]QOSA_EASY_SMS_INIT_OK", simid);
            qosa_sem_release(g_easy_sms_init_semaphore);
            break;
        }

        case QOSA_EASY_SMS_MEM_FULL: {
            qosa_sms_stor_e *msg = (qosa_sms_stor_e *)ctx;
            QLOGI("QOSA_EASY_SMS_MEM_FULL [%d]mem:%d", simid, *msg);
            break;
        }
        
        case QOSA_EASY_SMS_REPORT: {
            qosa_sms_record_t *record = (qosa_sms_record_t *)ctx;
            qosa_sms_msg_t     msg = {0};
            qosa_sms_rept_t   *msg_rept = qosa_malloc(sizeof(qosa_sms_rept_t));
            qosa_uint16_t      rsp_len = 0;
            char              *rsp = qosa_calloc(1, 1024);
            if (QOSA_NULL == msg_rept || QOSA_NULL == rsp)
            {
                QLOGE("malloc error");
                return;
            }

            // Parse PDU data into TEXT data.
            qosa_sms_pdu_to_text(record, &msg);
            qosa_memcpy(msg_rept, &msg.text.report, sizeof(qosa_sms_rept_t));
            
            // MT(Mobile Terminated) phone number */
            rsp_len += qosa_snprintf(rsp + rsp_len, 1024 - rsp_len, ",\"%s\"", msg_rept->ra);

            // Send Time
            rsp_len += qosa_snprintf(
                rsp + rsp_len,
                1024 - rsp_len,
                ",\"%02d/%02d/%02d,%02d:%02d:%02d+%02d\"",
                msg_rept->scts.year,
                msg_rept->scts.month,
                msg_rept->scts.day,
                msg_rept->scts.hour,
                msg_rept->scts.minute,
                msg_rept->scts.second,
                msg_rept->scts.zone
            );
            
            // Arrival Time
            rsp_len += qosa_snprintf(
                rsp + rsp_len,
                1024 - rsp_len,
                ",\"%02d/%02d/%02d,%02d:%02d:%02d+%02d\"",
                msg_rept->dt.year,
                msg_rept->dt.month,
                msg_rept->dt.day,
                msg_rept->dt.hour,
                msg_rept->dt.minute,
                msg_rept->dt.second,
                msg_rept->dt.zone
            );
            
            QLOGI("[%d]QOSA_EASY_SMS_REPORT:%s", simid, rsp);
            qosa_free(rsp);
            qosa_free(msg_rept);
            break;
        }
        default:
            break;
    }
}

/**
 * @brief Easy SMS uers read TEXT SMS callback.
 *
 * @param[in] simid
 *          - SIM ID
 * @param[in] event_id
 *          - event ID
 * @param[in] ctx
 *          - context
 * @return void
 */
void user_receive_sms_text_callback(qosa_uint8_t simid, qosa_uint8_t event_id, void *ctx)
{
    QOSA_UNUSED(event_id);
    qosa_sms_record_t *record = (qosa_sms_record_t *)ctx;
    qosa_sms_msg_t     msg = {0};
    qosa_sms_recv_t   *msg_text = qosa_malloc(sizeof(qosa_sms_recv_t));
    qosa_uint16_t      rsp_len = 0;
    char              *rsp = qosa_calloc(1, 1024);
    if (QOSA_NULL == msg_text || QOSA_NULL == rsp)
    {
        QLOGE("malloc error");
        return;
    }
    // Parse PDU data into TEXT data.
    qosa_sms_pdu_to_text(record, &msg);
    qosa_memcpy(msg_text, &msg.text.recv, sizeof(qosa_sms_recv_t));

    // MO(Mobile Originated) phone number
    rsp_len += qosa_snprintf(rsp, 1024, "%s,", msg_text->oa);

    // SMS content
    rsp_len += qosa_snprintf(rsp + rsp_len, 1024 - rsp_len, "%s,", msg_text->data);

    // Send Time
    rsp_len += qosa_snprintf(
        rsp + rsp_len,
        1024 - rsp_len,
        "\"%02d/%02d/%02d,%02d:%02d:%02d+%02d\",",
        msg_text->scts.year,
        msg_text->scts.month,
        msg_text->scts.day,
        msg_text->scts.hour,
        msg_text->scts.minute,
        msg_text->scts.second,
        msg_text->scts.zone
    );
    
    // Encoding Method  0-GSM7, 8-UCS2
    rsp_len += qosa_snprintf(rsp + rsp_len, 1024 - rsp_len, "%d", msg_text->dcs);

    QLOGI("[%d]text msg:%s", simid, rsp);
    qosa_free(rsp);
    qosa_free(msg_text);
}

/**
 * @brief Easy SMS uers read PDU SMS callback.
 *
 * @param[in] simid
 *          - SIM ID
 * @param[in] event_id
 *          - event ID
 * @param[in] ctx
 *          - context
 * @return void
 */
void user_receive_sms_pdu_callback(qosa_uint8_t simid, qosa_uint8_t event_id, void *ctx)
{
    QOSA_UNUSED(event_id);
    qosa_sms_record_t *record = (qosa_sms_record_t *)ctx;
    qosa_uint8_t       data_len_without_sca = 0;
    //Raw PDU data
    qosa_sms_pdu_t *msg_pdu = qosa_malloc(sizeof(qosa_sms_pdu_t));
    qosa_uint16_t   rsp_len = 0;
    char           *rsp = qosa_calloc(1, 1024);
    if (QOSA_NULL == msg_pdu || QOSA_NULL == rsp)
    {
        QLOGE("malloc error");
        return;
    }
    qosa_memcpy(msg_pdu, &record->pdu, sizeof(qosa_sms_pdu_t));

    // PDU excluding the SCA portion
    data_len_without_sca = msg_pdu->data_len - QOSA_SMS_GET_SCA_PART_LEN(msg_pdu->data);
    rsp_len += qosa_snprintf(rsp, 1024, "%d,", data_len_without_sca);
    qcm_utils_data_to_hex_string(msg_pdu->data, msg_pdu->data_len, rsp + rsp_len);

    QLOGI("[%d]pdu msg:%s", simid, rsp);
    qosa_free(rsp);
    qosa_free(msg_pdu);
}

/**
 * @brief Main function for SMS demonstration task
 *
 * @param[in] param
 *          - Task parameter pointer (unused)
 * @return void
 * @note Initializes event callbacks and enters main demo.
 * Using this API does not run concurrently with AT; Do not call any other SMS APIs while this function is invoked.
 */
void qosa_easy_sms_demo_task(void *param)
{
    QOSA_UNUSED(param);
    qosa_uint8_t            simid = 0;
    qosa_easy_sms_errcode_e ret = 0;
    QLOGI("enter");
    // Wait for SMS initialization to complete.
    if (qosa_sem_create(&g_easy_sms_init_semaphore, 0) != QOSA_OK)
    {
        QLOGE("sms_init_sem created failed");
        goto exit;
    }

    // Register SMS easy event callback.
    qosa_easy_sms_register_event_cb(user_sms_event_callback);
    qosa_easy_sms_receive_text_cb_register(user_receive_sms_text_callback);
    qosa_easy_sms_receive_pdu_cb_register(user_receive_sms_pdu_callback);

    uint8_t sim_init_status = qosa_sim_read_init_stat(simid);
    QLOGI("sim_init_status:%d", sim_init_status);
    if (!((sim_init_status >> 2) & 1))
    {
        if (qosa_sem_wait(g_easy_sms_init_semaphore, QOSA_WAIT_FOREVER))
        {
            QLOGE("Waiting for SMS init timeout");
            goto exit;
        }
    }
    QLOGI("=== EASY SMS Demo STATR ===");
    
    // Send message.
    // case 1: TEXT SMS
    // Note! TEXT messages must use UCS-2 encoding format, and the input string is a pre-compiled UCS-2 string.
    qosa_easy_sms_param_t params = QOSA_SMS_PARAM_DEFAULT_INIT;
    qosa_easy_sms_set_param(&params);
    ret = qosa_easy_sms_send_text_msg("10086", "4E00952E0053004D0053FF0C79FB8FDCFF01");
    if (QOSA_EASY_SMS_SUCCESS != ret)
    {
        QLOGE("Send TEXT SMS FAIL!");
    }
    else
    {
        QLOGI("Send TEXT SMS SUCCESS!");
    }

    // case 2: PDU SMS
    // Note! PDU SMS requires entering the correct actual PDU data length.
    qosa_task_sleep_sec(10);
    params.format = QOSA_EASY_SMS_PDU;
    qosa_easy_sms_set_param(&params);
    ret = qosa_easy_sms_send_pdu_msg("00310005910180F60000FF12C5E0340B9A36A72C50B45A1C528BCC10", 27);
    if (QOSA_EASY_SMS_SUCCESS != ret)
    {
        QLOGE("Send PDU SMS FAIL!");
    }
    else
    {
        QLOGI("Send PDU SMS SUCCESS!");
    }
    // Delete message.
    // Note! This API will delete all messages.
    qosa_task_sleep_sec(10);
    ret = qosa_easy_sms_delete_msg();
    if (QOSA_EASY_SMS_SUCCESS != ret)
    {
        QLOGE("Delete SMS FAIL!");
    }
    else
    {
        QLOGI("Delete SMS SUCCESS!");
    }

exit:
    QLOGI("=== EASY SMS Demo Finished ===");
    if (g_easy_sms_init_semaphore)
    {
        qosa_sem_delete(g_easy_sms_init_semaphore);
    }
    if (g_easy_sms_demo_task_ptr != QOSA_NULL)
    {
        qosa_task_delete(g_easy_sms_demo_task_ptr);
        g_easy_sms_demo_task_ptr = QOSA_NULL;
    }
    return;
}

/**
 * @brief Initialize SMS demonstration program
 *
 * @return void
 * @note This function sets up SMS configuration, event callbacks, and creates necessary tasks for the demo.
 * Using this API does not run concurrently with AT; Do not call any other SMS APIs while this function is invoked.
 *
 */
void unir_easy_sms_demo_init(void)
{
    int err = 0;

    // SMS_easy configuration initialization.
    qosa_easy_sms_init_config();

    QLOGI("enter UniRTOS SMS easy DEMO !!!");
    if (g_easy_sms_demo_task_ptr == QOSA_NULL)
    {
        err = qosa_task_create(&g_easy_sms_demo_task_ptr, 4 * 1024, QOSA_PRIORITY_NORMAL, "easy_sms_demo", qosa_easy_sms_demo_task, QOSA_NULL);
        if (err != QOSA_OK)
        {
            QLOGE("SMS easy demo init task create error");
            return;
        }
    }
    return;
}
