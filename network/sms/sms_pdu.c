//send pdu sms

static void qosa_sms_demo_send_msg_rsp(void *ctx, void *argv)
{
    qosa_sms_send_pdu_cnf_t *cnf = argv;

    QLOGI("result sim:%d err:0x%x", qosa_sms_simid, cnf->err_code);

    if (QOSA_SMS_SUCCESS != cnf->err_code)
    {
        QLOGI("Send SMS failed with error:%d", cnf->err_code);
    }
    else
    {
        QLOGI("Send SMS success, MR:%u", cnf->mr);
    }
}

//qosa_sms_demo_send_pdu_sms("0891683110501905F0110005910180F60000FF060000BD3CA703",17)
static int qosa_sms_demo_send_pdu_sms(const char *message_txt, qosa_uint32_t pdu_length)
{
    int                     qosa_err = QOSA_SMS_SUCCESS;
    qosa_bool_t             is_attached = 0;
    qosa_uint16_t           message_len = 0;
    qosa_sms_send_param_t   send_param = {0};
    qosa_uint8_t            *pdu_with_sca = QOSA_NULL;
    qosa_uint8_t            pdu_with_sca_len = 0;
    qosa_uint8_t            sca_len = 0;

    if( (message_txt == QOSA_NULL) || (pdu_length <= 0) )
    {
        return -1;
    }

    is_attached = qosa_datacall_wait_attached(qosa_sms_simid, QOSA_SMS_DEMO_WAIT_ATTACH_TIMEOUT);
    if (!is_attached)
    {
        QLOGI("attach fail");
        return -1;
    }

    message_len = qosa_strlen(message_txt);
    if (message_len == 0)
    {
        return -1;
    }
    do {
        // convert text message to pdu message
        pdu_with_sca = qosa_malloc(message_len);
        if ((pdu_with_sca == QOSA_NULL) || (QOSA_OK != qcm_utils_data_string_to_hex((const char *)message_txt, message_len, pdu_with_sca)))
        {
            qosa_err = QOSA_SMS_CMS_INVALID_PARA;
            qosa_free(pdu_with_sca);
            pdu_with_sca = QOSA_NULL;
            break;
        }

        sca_len = QOSA_SMS_GET_SCA_PART_LEN(pdu_with_sca);
        if ((sca_len + pdu_length) != message_len / 2)
        {
            qosa_err = QOSA_SMS_CMS_INVALID_PARA;
            qosa_free(pdu_with_sca);
            pdu_with_sca = QOSA_NULL;
            break;
        }
        pdu_with_sca_len = message_len / 2;

        send_param.pdu.data_len = pdu_with_sca_len;
        qosa_memcpy(send_param.pdu.data, pdu_with_sca, pdu_with_sca_len);
        qosa_err = qosa_sms_send_pdu_async(qosa_sms_simid, &send_param, qosa_sms_demo_send_msg_rsp, pdu_with_sca);
    } while (0);

    if(QOSA_SMS_SUCCESS != qosa_err)
    {
        QLOGI("SEND SMS FAILED qosa_err:%d",qosa_err);
        return -1;
    }

    return 0;

}