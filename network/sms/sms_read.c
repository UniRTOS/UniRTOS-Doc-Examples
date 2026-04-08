static void qosa_sms_demo_read_msg_rsp(void *ctx, void *argv)
{
    char                        *sms_message = QOSA_NULL;
    qosa_sms_msg_t              msg = {0};
    qosa_sms_read_pdu_cnf_t     *cnf = argv;
    qosa_sms_record_t           *record = &cnf->record;
    qosa_sms_demo_format_type_e format = QOSA_SMS_DEMO_FORMAT_PDU;
    qosa_sms_send_t             *send = QOSA_NULL;
    qosa_sms_recv_t             *recv = QOSA_NULL;
    qosa_sms_rept_t             *report = QOSA_NULL;
    qosa_sms_pdu_t              *pdu = QOSA_NULL;

    sms_message = qosa_calloc(1, 1024);
    if (sms_message == QOSA_NULL)
    {
        QLOGI("calloc failed");
        return;
    }
    QLOGI("result sim:%d err:0x%x", qosa_sms_simid, cnf->err_code);

    format = qosa_sms_cfg.format[qosa_sms_simid];

    if (QOSA_SMS_SUCCESS != cnf->err_code)
    {
        if (QOSA_SMS_NO_MSG_ERR == cnf->err_code)
        {
            QLOGI("Read SMS success");
            return;
        }
        else
        {
            QLOGI("Read SMS failed");
            return;
        }
    }
    else
    {        pdu = &record->pdu;

        qosa_memset(sms_message, 0, 1024);
        if (QOSA_SMS_DEMO_FORMAT_TEXT == format)
        {
            qosa_sms_pdu_to_text(record, &msg);
            send = &msg.text.send;
            recv = &msg.text.recv;
            report = &msg.text.report;

            switch (msg.msg_type)
            {
                case QOSA_SMS_SUBMIT:
                    {
                        qosa_snprintf(sms_message, 1024, "\"%s\", \"%s\", \"%s\"", qosa_sms_demo_sms_msg_status_str(send->status), send->da, (const char *)send->data);
                        QLOGI("SMS: %s",sms_message);
                    }
                    break;

                case QOSA_SMS_DELIVER:
                    {                        qosa_snprintf(sms_message, 1024, "\"%s\", \"%s\", \"%u/%02u/%02u,%02u:%02u:%02u\", \"%s\"",
                                                qosa_sms_demo_sms_msg_status_str(recv->status), recv->oa, recv->scts.year,
                                                recv->scts.month, recv->scts.day, recv->scts.hour,
                                                    recv->scts.minute, recv->scts.second, (const char *)recv->data);

                        QLOGI("SMS: %s",sms_message);
                    }
                    break;

                case QOSA_SMS_STATUS_REPORT:
                    {
                        qosa_snprintf(sms_message, 1024, "\"%s\", \"%s\"", qosa_sms_demo_sms_msg_status_str(report->status), report->ra);
                        QLOGI("SMS: %s",sms_message);
                    }
                    break;

                default:
                    break;
            }
        }
        else        {
            // PDU format
            qcm_utils_data_to_hex_string(pdu->data, pdu->data_len, sms_message);
            QLOGI("SMS: %u, %u, %s", record->status, pdu->data_len, (const char *)sms_message);
        }

        }

    if (sms_message)
        {
        qosa_free(sms_message);
        sms_message = QOSA_NULL;
        }

    return ;
}

static int qosa_sms_demo_read_sms(qosa_uint16_t index)
{
    int                         qosa_err = QOSA_SMS_SUCCESS;
    qosa_sms_stor_e             storage = QOSA_SMS_STOR_ME;
    qosa_uint16_t               total_slot = 0;
    qosa_sms_stor_info_t        stor_info = {0};
    qosa_sms_demo_mem_info_t    mem_info = {0};
    qosa_sms_read_param_t       read = {0};

    if(index < 0)
    {
        QLOGI("index error");
        return -1;
    }

    qosa_err = qosa_sms_demo_get_mem(qosa_sms_simid, &mem_info);
    if (QOSA_SMS_SUCCESS != qosa_err)
    {
        QLOGI("get memory storage failed");
        return -1;
    }
    storage = mem_info.mem1;

    if (QOSA_SMS_SUCCESS != qosa_sms_get_stor_info(qosa_sms_simid, &stor_info))
    {
        QLOGI("get memory storage failed");
        return -1;
    }

    // read method use mem1
    total_slot = (QOSA_SMS_STOR_ME == storage) ? stor_info.total_me : stor_info.total_sm;

    if (index > total_slot - 1)
    {
        QLOGI("no index SMS");
        return -1;
    }

    read.stor = storage;
    read.index = index;
    qosa_err = qosa_sms_read_pdu_async(qosa_sms_simid, &read, qosa_sms_demo_read_msg_rsp, QOSA_NULL);
    if (QOSA_SMS_SUCCESS != qosa_err)
    {        if (QOSA_SMS_NO_MSG_ERR == qosa_err)
        {
            QLOGI("SMS read success");
            return 0;
        }
        else
        {
            QLOGI("SMS read failed");
            return -1;
        }
    }

    return 0;
}