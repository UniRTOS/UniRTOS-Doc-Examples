int main(int argc, char *argv[])
{
    qosa_uint8_t simid = 0;
    qapp_easy_nw_init_acc_params_t tpl = 
    {
        b.enable = QOSA_TRUE,
        rat.tpl = QAPP_EASY_NW_INIT_ACC_TPL_RAT_PRIO_5G_4G_3G_2G,
        plmn.tpl = QAPP_EASY_NW_INIT_ACC_TPL_PLMN_PREF_CHINA_MOBILE,
        band.tpl = QAPP_EASY_NW_INIT_ACC_TPL_BAND_PREF_FDD_ONLY,
        fdd_tdd.tpl = QAPP_EASY_NW_INIT_ACC_TPL_FDD_PREF,
    };
    qapp_easy_nw_init_acc_tpl_set_config(&tpl, QAPP_EASY_NW_INIT_ACC_TPL_USER_0);
    qapp_easy_nw_init_acc_tpl_write(simid, pdp_id, QAPP_EASY_NW_INIT_ACC_TPL_USER_0);
    return 0;
}