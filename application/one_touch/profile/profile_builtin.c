int main(int argc, char *argv[])
{
    qosa_uint8_t simid = 0;
    qosa_uint8_t pdp_id = 1;
    // 将【内置模板中国移动】的拨号参数，下发写入到指定SIM卡、指定PDP链路中，完成拨号参数配置生效
    qapp_easy_nw_datacall_prof_tpl_write(sim_id, pdp_id, QAPP_EASY_NW_DATACALL_PROF_TPL_INTERNAL_CMNET_IPV4V6);
}