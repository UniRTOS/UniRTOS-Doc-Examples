osa_task_t g_lpa_rsp_task = OSA_NULL;

typedef struct
{
    char ac_code[ESIM_AC_CC_CODE_MAX_LEN];
    char cc_code[ESIM_AC_CC_CODE_MAX_LEN];
}lpa_rsp_arg_t;

static void ql_esim_lpa_rsp_start_main(void *arg)
{
    esim_result_errno_e ret = ESIM_OK;
    osa_task_t tsak_tmp = NULL;
    esim_data_array_t iccid_str = {0};

    lpa_rsp_arg_t *argv = (lpa_rsp_arg_t *)arg;
    if(argv != NULL)
    {

        ret = esim_intf_direct_download_profile(argv->ac_code, osa_strlen(argv->ac_code), argv->cc_code, osa_strlen(argv->cc_code), &iccid_str);
        if(ret = ESIM_OK)
        {
            utils_printf("download success, iccid:%s", iccid_str->data);
        }
        else
        {
            utils_printf("download fail ,ret: %X", ret);
        }
        osa_free(argv);
    }
    tsak_tmp = g_lpa_rsp_task;
    g_lpa_rsp_task = NULL;
    osa_task_delete(tsak_tmp);
}

static esim_result_errno_e ql_esim_lpa_profile_download(lpa_rsp_arg_t *argv)
{
    esim_result_errno_e ret = ESIM_OK;
    int osa_err = 0;

    osa_err = osa_task_create((osa_task_t*)&g_lpa_rsp_task,
                    10240,
                    OSA_PRIORITY_ABOVE_NORMAL,
                    "ESIM_RSP_THREAD",
                    &ql_esim_lpa_rsp_start_main,
                    (void *)argv);
    if(osa_err != 0)
    {
        osa_free(argv);
        ret = ESIM_ERROR_GENERAL;
        utils_printf("create rsp task  error ");
    }

    return ret;
}