#include "qosa_def.h"

#define NETWORK_SELECTION_DEMO_SIM_ID 0

static void network_selection_demo_init(void)
{
    qosa_uint8_t sim_id = NETWORK_SELECTION_DEMO_SIM_ID;
    qapp_easy_nw_init_acc_params_t tpl = 
    {
        .b.enable = QOSA_TRUE,
        .rat.tpl = QAPP_EASY_NW_INIT_ACC_TPL_RAT_PRIO_5G_4G_3G_2G,
        .plmn.tpl = QAPP_EASY_NW_INIT_ACC_TPL_PLMN_PREF_CHINA_MOBILE,
        .band.tpl = QAPP_EASY_NW_INIT_ACC_TPL_BAND_PREF_FDD_ONLY,
        .fdd_tdd.tpl = QAPP_EASY_NW_INIT_ACC_TPL_FDD_PREF,
    };
    qapp_easy_nw_init_acc_tpl_set_config(&tpl, QAPP_EASY_NW_INIT_ACC_TPL_USER_0);
    qapp_easy_nw_init_acc_tpl_write(sim_id, QAPP_EASY_NW_INIT_ACC_TPL_USER_0);
}