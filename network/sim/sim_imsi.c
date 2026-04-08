// 同步获取IMSI
int sim_demo_sim_imsi_get_sync(qosa_uint8_t simid)
{
    qosa_sim_imsi_t imsi = {0};
    int ret = 0;
    ret = qosa_sim_read_imsi(simid, &imsi);
    if (ret == QOSA_SIM_ERR_OK)
    {
        QLOGI("[sync] read IMSI: %s", imsi.imsi);
    }
    else
    {
        QLOGE("[sync] Read IMSI failed: 0x%x", ret);
    }
    return ret;
}

/**
 * @brief SIM卡获取IMSI异步回调函数
 * 
 * @param [in] ctx 
 *           - 回调上下文参数，本函数中未使用。
 * @param [in] argv 
 *           - 回调参数指针，指向 qosa_sim_get_imsi_cnf_t 结构体
 * 
 * @return 无返回值
 */
static void sim_demo_get_imsi_cb(void *ctx, void *argv)
{
    qosa_sim_get_imsi_cnf_t *cnf = argv;
    QOSA_UNUSED(ctx);

    // 检查获取 IMSI 结果
    if (QOSA_SIM_ERR_OK == cnf->err_code)
    {
        // 操作成功，打印获取的IMSI号码
        QLOGI("[async] get IMSI: %s", cnf->imsi.imsi);
    }
    else
    {
        // 操作失败，打印错误代码
        QLOGI("[async] get IMSI fail: %x", cnf->err_code);
    }
}

// 异步获取IMSI
int sim_demo_sim_imsi_get_async(qosa_uint8_t simid)
{
    int ret = 0;
    qosa_int32_t qosa_err = 0;
    
    // 异步获取 IMSI
    qosa_err = qosa_sim_get_imsi(simid, sim_demo_get_imsi_cb, QOSA_NULL);
    if (QOSA_SIM_ERR_OK != qosa_err)
    {
        QLOGE("qosa_sim_get_imsi failed: 0x%x", qosa_err);
        ret = -1;
    }
    return ret;
}
