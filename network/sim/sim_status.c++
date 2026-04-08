// 检查SIM的状态信息，包括插入状态，缓冲状态，功能块初始化状态
int sim_demo_sim_status_check(qosa_uint8_t simid)
{
    qosa_sim_status_e status = QOSA_SIM_STATUS_READY;
    qosa_sim_imsi_t imsi = {0};
    int             ret = 0;

    // 检查SIM卡插入状态
    ret = qosa_sim_read_insert_stat(simid);
    if (ret != QOSA_SIM_INSERT_STAT_INSERTED)
    {
        QLOGE("Read SIM insert status failed: 0x%x", ret);
        return ret;
    }
    QLOGI("SIM insert status: 0x%x", ret);


    // 读取缓冲的SIM卡状态
    ret = qosa_sim_read_status(simid, &status);
    if (ret != QOSA_SIM_ERR_OK)
    {
        QLOGE("Read SIM status failed: 0x%x", ret);
        return ret;
    }
    QLOGI("SIM Status: 0x%x", status);

    // 检查功能初始化状态
    ret = qosa_sim_read_init_stat(simid);
    QLOGI("SIM init status: 0x%x", ret);

    return ret;
}