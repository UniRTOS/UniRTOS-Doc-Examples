// 设置热插拔
int sim_demo_sim_hot_swap_config(qosa_uint8_t simid, qosa_bool_t enable)
{
    qosa_sim_hot_swap_cfg_t hot_swap_cfg = {0};
    int ret = QOSA_SIM_ERR_OK;

    // 查询 SIM 卡热插拔配置
    ret = qosa_sim_get_sim_hot_swap(simid, &hot_swap_cfg);
    if (QOSA_SIM_ERR_OK != ret)
    {
        QLOGE("get sim hot swap error: 0x%x", ret);
        return ret ;
    }
    QLOGI("hot swap enable:%d, insert_level:%d", hot_swap_cfg.enable, hot_swap_cfg.insert_level);
    if (hot_swap_cfg.enable == enable)
    {
        ret = QOSA_SIM_ERR_OK;
    }
    else
    {
        // 设置 SIM 卡热插拔配置
        hot_swap_cfg.enable = enable;
        hot_swap_cfg.insert_level = 1;
        hot_swap_cfg.gpio = QOSA_SIM_HOT_SWAP_UNSPECIFIED_GPIO;
        ret = qosa_sim_set_sim_hot_swap(simid, &hot_swap_cfg);
        if (QOSA_SIM_ERR_OK != ret)
        {
            QLOGE("set sim hot swap error: 0x%x", ret);
        }
    }
    return ret;
}