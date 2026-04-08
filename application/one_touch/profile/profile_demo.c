int main(int argc, char *argv[])
{
    qosa_uint8_t simid = 0;
    qosa_uint8_t pdp_id = 1;
    qapp_easy_nw_datacall_prof_param_t tpl = {
        // PDP上下文核心配置参数段
        .pdp.apn_valid = QOSA_TRUE,            /**< APN配置项生效开关：开启，本次配置的APN内容有效 */
        .pdp.apn = "test",                     /**< 接入点名称(APN)：配置为测试用APN名称 test */
        .pdp.pdp_type = QOSA_PDP_TYPE_IPV4V6,  /**< PDP协议类型：配置为IPv4和IPv6双栈模式 */
        .pdp.ipv4_ip_valid = QOSA_FALSE,       /**< IPv4地址配置项生效开关：关闭，不启用自定义IPv4地址 */
        .pdp.ipv6_ip_valid = QOSA_FALSE,       /**< IPv6地址配置项生效开关：关闭，不启用自定义IPv6地址 */
        .pdp.data_comp_valid = QOSA_FALSE,     /**< 数据压缩功能开关：关闭，不启用数据压缩 */
        .pdp.head_comp_valid = QOSA_FALSE,     /**< 报文头压缩功能开关：关闭，不启用头压缩 */
        .pdp.ipv4_addr_alloc_valid = QOSA_FALSE, /**< IPv4地址分配方式配置开关：关闭，使用默认分配方式 */
        .pdp.request_type_valid = QOSA_FALSE,  /**< 请求类型配置项生效开关：关闭，使用默认请求类型 */
        .pdp.secpco_valid = QOSA_TRUE,         /**< 安全协议配置项生效开关：开启，本次配置的secpco值有效 */
        .pdp.secpco = 1,                       /**< 安全协议配置值：配置为1（对应指定安全协议版本） */
    
        // PDP拨号鉴权相关配置参数段
        .auth.auth_valid = QOSA_TRUE,          /**< 鉴权配置项生效开关：开启，本次鉴权配置内容有效 */
        .auth.auth_type = QOSA_PDP_AUTH_TYPE_NONE, /**< 鉴权方式：配置为无鉴权模式 */
        .auth.user_valid = QOSA_FALSE,         /**< 鉴权用户名配置生效开关：关闭，不配置鉴权用户名 */
        .auth.pass_valid = QOSA_FALSE,         /**< 鉴权密码配置生效开关：关闭，不配置鉴权密码 */
    };
    // 将配置好的PDP参数模板，写入到【用户自定义模板0】中
    qapp_easy_nw_datacall_prof_tpl_set_config(&tpl, QAPP_EASY_NW_DATACALL_PROF_TPL_USER_0);
    // 将【用户自定义模板0】的拨号参数，下发写入到指定SIM卡、指定PDP链路中，完成拨号参数配置生效
    qapp_easy_nw_datacall_prof_tpl_write(sim_id, pdp_id, QAPP_EASY_NW_DATACALL_PROF_TPL_USER_0);
}