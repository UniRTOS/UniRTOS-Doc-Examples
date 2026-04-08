int main(int argc, char *argv[])
{
    qosa_uint8_t simid = 0;
    // 将【内置默认模板】的拨号参数，下发写入到指定SIM卡、指定PDP链路中，完成拨号参数配置生效
    qapp_easy_nw_blacklist_tpl_write(sim_id, pdp_id, QAPP_EASY_NW_BLACKLIST_TPL_INTERNAL_DEFAULT);
}