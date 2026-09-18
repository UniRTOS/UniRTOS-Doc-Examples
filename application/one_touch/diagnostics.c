#include "qosa_def.h"
#include "qosa_log.h"
#include "easy_network_diagnostics.h"
/*===========================================================================
 * Macro Definition
 ===========================================================================*/
#define QOS_LOG_TAG                         LOG_TAG_DEMO

/*========================================================================
*  Enumeration Definition
*========================================================================*/
/*========================================================================
*  Structure Definition
*========================================================================*/

/*========================================================================
*  Global Variable
*========================================================================*/
qosa_task_t         g_easy_network_diagnostics_test_task = QOSA_NULL;
qapp_network_diagnostics_config_t g_nw_diagnostics_config =
{
    .rsrp_threshold = QAPP_NW_DIAGNOSTICS_RSRP_THRESHOLD_DEFAULT,
    .rsrq_threshold = QAPP_NW_DIAGNOSTICS_RSRQ_THRESHOLD_DEFAULT,
    .snr_threshold = QAPP_NW_DIAGNOSTICS_SNR_THRESHOLD_DEFAULT,

    .ul_bler_threshold = QAPP_NW_DIAGNOSTICS_UL_BLER_THRESHOLD_DEFAULT,
    .dl_bler_threshold = QAPP_NW_DIAGNOSTICS_DL_BLER_THRESHOLD_DEFAULT
};
/*========================================================================
*  Func Definition
*========================================================================*/
static void easy_network_diagnostics_demo_task(void *arg)
{
    QOSA_UNUSED(arg);

    qosa_task_sleep_sec(3);
    QLOGI("enter easy_network_diagnostics_demo_task");

    while (1)
    {
        qapp_nw_diagnostics_result result = {0};
        qapp_network_diagnostics_config_t *config = &g_nw_diagnostics_config;
        result = qapp_easy_nw_diagnostics(config);
        QLOGI("result = %x",result);
        qosa_task_sleep_sec(5);
    }
}

void unir_easy_network_diagnostics_demo_init()
{
    int err = 0;
    QLOGI("enter easy_network_diagnostics_demo_init");
    // Create easy network_diagnostics demo task
    err = qosa_task_create(&g_easy_network_diagnostics_test_task, 1024 * 4, QOSA_PRIORITY_NORMAL, "EZNWDNTS", easy_network_diagnostics_demo_task, QOSA_NULL);
    if (err != QOSA_OK)
    {
        QLOGD("easy network diagnostics demo task create error");
        return;
    }
}

