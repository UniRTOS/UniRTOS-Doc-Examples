//send Chinese characters sms

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

//6D4B8BD50074006500730074 == 测试test
//qosa_sms_demo_send_all_characters_sms("10086", "6D4B8BD50074006500730074")
static int qosa_sms_demo_send_all_characters_sms(const char *phone_number, const char *message_txt)
{
    int                     qosa_err = QOSA_SMS_SUCCESS;
    qosa_bool_t             is_attached = 0;
    qosa_uint16_t           message_len = 0;
    qosa_sms_msg_t          message = {0};
    qosa_sms_send_param_t   send_param = {0};
    qosa_sms_cfg_t          sms_conf = {0};
    qosa_uint8_t            *pdu_with_sca = QOSA_NULL; // Free memory in PDU mode only, TEXT mode excludes freeing.
    qosa_uint8_t            pdu_with_sca_len = 0;
    qosa_sms_record_t       record = {0};

    if( (message_txt == QOSA_NULL) || (phone_number == QOSA_NULL) )
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

    //DA support QOSA_CS_GSM charset only.
    qosa_sms_set_charset(QOSA_CS_GSM);
    // Fill in basic information for text messages.
    message.msg_type = QOSA_SMS_SUBMIT;
    message.text.send.status = 0xff;
    qosa_strcpy(message.text.send.da, (const char *)phone_number);
    message.text.send.toda = 129;
    qosa_strcpy((char *)message.text.send.data, (const char *)message_txt);
    message.text.send.data_len = message_len;
    // DATA support QOSA_CS_UCS2 charset
    message.text.send.data_chset = QOSA_CS_UCS2;
    
    // Copy the text message configuration information.
    qosa_sms_get_config(g_sms_simid, &sms_conf);
    message.text.send.fo = sms_conf.text_fo;
    message.text.send.pid = sms_conf.text_pid;
    message.text.send.vp = sms_conf.text_vp;

    // If the messages is UCS2 encoding. the PDU is configured as 16bit, the dcs set 0x08
    message.text.send.dcs = 0x08;

    // Copy concatenated SMS information.
    message.is_concatenated = QOSA_FALSE;
    message.concat.msg_ref_number = 0;
    message.concat.msg_seg = 0;
    message.concat.msg_total = 0;
    do {
        // Convert text message to pdu message
        qosa_err = qosa_sms_text_to_pdu(&message, &record);
        if (qosa_err != QOSA_SMS_SUCCESS)
        {
            break;
        }
        pdu_with_sca = record.pdu.data;
        pdu_with_sca_len = record.pdu.data_len; 
    
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