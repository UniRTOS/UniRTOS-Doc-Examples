
// 同步获取ICCID
int sim_demo_sim_iccid_get_sync(qosa_uint8_t simid)
{
    qosa_sim_iccid_t  iccid = {0};
    int               ret = 0;
    ret = qosa_sim_read_iccid(simid, &iccid);
    if (ret == QOSA_SIM_ERR_OK)
    {
        QLOGI("[sync] read ICCID: %s", iccid.id);
    }
    else
    {
        QLOGE("[sync] Read ICCID failed: 0x%x", ret);
    }
    return ret;
}

/**
 * @brief SIM卡获取ICCID异步回调函数
 * 
 * @param [in] ctx 
 *           - 回调上下文参数，本函数中未使用。
 * @param [in] argv 
 *           - 回调参数指针，指向 qosa_sim_get_iccid_cnf_t 结构体
 * 
 * @return 无返回值
 */
static void sim_demo_get_iccid_cb(void *ctx, void *argv)
{
    qosa_sim_get_iccid_cnf_t *cnf = argv;
    QOSA_UNUSED(ctx);

    // 检查获取ICCID结果
    if (QOSA_SIM_ERR_OK == cnf->err_code)
    {
        QLOGI("[async] get ICCID: %s", cnf->iccid.id);
    }
    else
    {
        QLOGI("[async] get ICCID fail: %x", cnf->err_code);
    }
}

// 异步获取ICCD
int sim_demo_sim_iccid_get_async(qosa_uint8_t simid)
{
    int ret = 0;
    qosa_int32_t qosa_err = 0;
    
    qosa_err = qosa_sim_get_iccid(simid, sim_demo_get_iccid_cb, QOSA_NULL);
    if (QOSA_SIM_ERR_OK != qosa_err)
    {
        QLOGE("qosa_sim_get_iccid failed: 0x%x", qosa_err);
        ret = -1;
    }
    return ret;
}
