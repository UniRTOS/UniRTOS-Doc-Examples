int main(int argc, char *argv[])
{
    qosa_uint8_t simid = 0;                                      // SIM卡ID，配置卡槽0
    // 初始化网络黑名单配置模板参数结构体
    qapp_easy_nw_blacklist_param_t tpl = {
        .blacklist.common.enable = QOSA_TRUE,                    // 使能网络黑名单总功能
        // 配置所有NAS事件的黑名单触发策略：超时自动解除+CFUN模式切换+射频下电 组合策略
        .blacklist.common.event_policy[QOSA_NW_NAS_RRC_REL_DURING_ATTACH ... QOSA_NW_NAS_EVENT_MAX - 1] = QOSA_NW_BLACKLIST_POLICY_TIMEOUT|QOSA_NW_BLACKLIST_POLICY_CFUN|QOSA_NW_BLACKLIST_POLICY_POWER_DOWN,
        .blacklist.common.stuck_enable = QOSA_TRUE,              // 使能黑名单防卡死保护功能
        .blacklist.common.stuck_timeout = 10,                    // 黑名单小区释放超时时间，单位：秒
        .blacklist.common.cnt_mode = QOSA_NW_BLACKLIST_CNT_MODE_CONSECUTIVE,  // 故障计数模式：连续故障计数模式
        .blacklist.common.cnt_thresh = 3,                        // 故障触发阈值：连续故障3次即加入黑名单
    };

    // 将配置好的黑名单参数 写入到 用户自定义黑名单模板0 中
    qapp_easy_nw_blacklist_tpl_set_config(&tpl, QAPP_EASY_NW_BLACKLIST_TPL_USER_0);
    // 将用户自定义黑名单模板0 的配置参数，下发写入到指定SIM卡和PDP链路，配置生效
    qapp_easy_nw_blacklist_tpl_write(simid, pdp_id, QAPP_EASY_NW_BLACKLIST_TPL_USER_0);
    
    return 0;
}