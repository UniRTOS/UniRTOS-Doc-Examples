/**
 * @brief SIM 示例设置 CFUN 回调函数
 *
 * @param[in] ctx
 *          - 上下文指针
 *
 * @param[in] argv
 *          - 回调参数指针，指向包含 CFUN 设置操作结果信息的 qosa_dev_general_cnf_t 结构体
 */
static void sim_demo_set_cfun_cb(void *ctx, void *argv)
{
    qosa_dev_general_cnf_t *cnf = argv;
    QLOGI("cfun result=%d", cnf->err_code);
    if(cnf->err_code == QOSA_SIM_ERR_OK)
    {
        // 释放 CFUN 信号量
        qosa_sem_release(g_sim_demo_sem);
    }
}


// 将指定的 SIM 卡模块设置为 CFUN=0（最小功能模式），然后切换到目标卡槽，然后恢复到 CFUN=1
static int sim_demo_dual_sim_switch_set_slot_id(qosa_uint8_t simid, qosa_uint8_t new_slot)
{
    int ret = QOSA_SIM_ERR_OK;
    int sem_ret = 0;

    // 将SIM卡模块设置为最小功能模式（CFUN=0）
    QLOGI("[CFUN]: 0");
    ret = qosa_dev_set_cfun(simid, 0, sim_demo_set_cfun_cb, QOSA_NULL);
    QLOGI("ret=%d,qosa_dev_set_cfun", ret);
    // 等待 CFUN=0 完成
    sem_ret = qosa_sem_wait(g_sim_demo_sem, 15000);
    if(sem_ret != QOSA_OK)
    {
        return QOSA_SIM_ERR_EXECUTE;
    }
    
    // 切换 SIM 卡槽
    ret = qosa_sim_set_slot_id(0, new_slot);
    if (ret == QOSA_SIM_ERR_OK)
    {
        QLOGI("Switched to SIM slot: %d success", new_slot);
        qosa_task_sleep_ms(1000);
    }
    else
    {
        QLOGI("Switched to SIM slot: %d fail,ret:%x", new_slot, ret);
        return ret;
    } 

    // Set SIM card module to full functional mode (CFUN=1)
    QLOGI("[CFUN]: 1");
    ret = qosa_dev_set_cfun(simid, 1, sim_demo_set_cfun_cb, QOSA_NULL);
    QLOGI("ret=%d,qosa_dev_set_cfun", ret);

    // Wait for CFUN=1 to complete
    sem_ret = qosa_sem_wait(g_sim_demo_sem, 15000);
    if(sem_ret != QOSA_OK)
    {
        return QOSA_SIM_ERR_EXECUTE;
    }

    return ret;
}


//demo universal semaphore
qosa_sem_t g_sim_demo_sem = QOSA_NULL;

// 获取当前使用的卡槽，切换到另一个卡槽
static int sim_demo_dual_sim_switch(qosa_uint8_t simid)
{
    qosa_uint8_t current_slot = 0;
    int ret = 0;
    int sem_ret = 0;

    // 创建演示通用信号量
    sem_ret  = qosa_sem_create(&g_sim_demo_sem, 0);
    if (sem_ret != QOSA_OK)
    {
        QLOGE("Create g_sim_demo_sem failed: %d", ret);
        return QOSA_SIM_ERR_EXECUTE;
    }
    
    // 获取当前使用的卡槽
    ret = qosa_sim_get_slot_id(0, &current_slot);
    if (ret != QOSA_SIM_ERR_OK)
    {
        QLOGE("Get SIM slot failed: 0x%x", ret);
    }
    else
    {
        QLOGI("Current SIM slot: %d", current_slot);
        // 设置切换卡槽（0/1）
        qosa_uint8_t new_slot = (current_slot == 0) ? 1 : 0;
        // 读取卡槽状态
        ret = qosa_sim_read_slot_stat(new_slot);
        if (ret != QOSA_SIM_INSERT_STAT_INSERTED)
        {
            QLOGE("SIM slot %d is not inserted.", new_slot);
            ret = QOSA_SIM_ERR_NOT_INSERTED;
        }
        else
        {
            // 执行卡槽切换操作
            ret = sim_demo_dual_sim_switch_set_slot_id(simid, new_slot);
            if (ret != QOSA_SIM_ERR_OK)
            {
                QLOGE("Switch SIM slot failed: 0x%x", ret);
            }
        }
    }
    qosa_sem_delete(g_sim_demo_sem)；
    g_sim_demo_sem = NULL;

    return ret;
}