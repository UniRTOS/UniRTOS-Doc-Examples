static void qosa_sms_demo_delete_msg_rsp(void *ctx, void *argv)
{
    qosa_sms_general_cnf_t *cnf = argv;

    QLOGI("Delete SMS result sim:%d err:0x%x", qosa_sms_simid, cnf->err_code);

    if (QOSA_SMS_SUCCESS != cnf->err_code)
    {
        if (QOSA_SMS_NO_MSG_ERR == cnf->err_code)
        {
            QLOGI("Delete SMS success");
            return;
        }
        else
        {
            QLOGI("Delete SMS failed");
            return;
        }
    }
    else
    {
        QLOGI("Delete SMS success");
        return;
    }
}

static int qosa_sms_demo_delete_sms(qosa_uint16_t index, qosa_sms_delflag_e delflag)
{
    int                         qosa_err = 0;
    qosa_uint16_t               total_slot = 0;
    qosa_sms_stor_e             storage = QOSA_SMS_STOR_ME;
    qosa_sms_demo_mem_info_t    mem_info = {0};
    qosa_sms_stor_info_t        stor_info = {0};
    qosa_sms_delete_param_t     delete = {0};

    if(index < 0)
    {
        QLOGI("index failed");
        return -1;
    }

    qosa_err = qosa_sms_demo_get_mem(qosa_sms_simid, &mem_info);
    if (QOSA_SMS_SUCCESS != qosa_err)
    {
        QLOGI("get memory storage failed");
        return -1;
    }

    storage = mem_info.mem1;

    // get storage iformation
    if (QOSA_SMS_SUCCESS != qosa_sms_get_stor_info(qosa_sms_simid, &stor_info))
    {
        QLOGI("get memory storage failed");
        return -1;
    }
    // delete method use mem1
    total_slot = (QOSA_SMS_STOR_ME == storage) ? stor_info.total_me : stor_info.total_sm;
    if(index > total_slot - 1)
    {
        QLOGI("no index SMS");
        return -1;
    }

    delete.index_or_stat = delflag == 0 ? QOSA_SMS_SELECT_BY_INDEX : QOSA_SMS_SELECT_BY_STATUS;
    delete.stor = storage;
    delete.index = index;
    delete.delflag = delflag;

    qosa_err = qosa_sms_delete_async(qosa_sms_simid, &delete, qosa_sms_demo_delete_msg_rsp, QOSA_NULL);
    if (QOSA_SMS_SUCCESS != qosa_err)
    {
        if (QOSA_SMS_NO_MSG_ERR == qosa_err)
        {
            QLOGI("SMS delete success");
            return 0;
        }
        else
        {
            QLOGI("SMS delete failed");
            return -1;
        }
    }

    return 0;
}