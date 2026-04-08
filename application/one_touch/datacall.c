#include "qosa_def.h"
#include "qosa_log.h"
#include "qosa_network.h"
#include "qosa_datacall.h"
#include "easy_datacall.h"
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
qosa_task_t         g_easy_datacall_test_task = QOSA_NULL;

qapp_easy_datacall_config_t g_easy_datacall_test_config1 =
{
    .pdp_config = {EASY_DATACALL_DEMO_DEFAULT_SIM_ID,EASY_DATACALL_DEMO_PDP_ID_1,
                    QOSA_DATACALL_CONN_TCPIP,EASY_DATACALL_DEMO_DEFAULT_APN,QOSA_PDP_IPV4V6,
                    QOSA_PDP_AUTH_TYPE_NONE,EASY_DATACALL_DEMO_DEFAULT_UN,EASY_DATACALL_DEMO_DEFAULT_PW},
    .retry_config = {EASY_DATACALL_DEMO_DEFAULT_RECONNECT_MODE,EASY_DATACALL_DEMO_DEFAULT_REG_TIMEOUT,
                    EASY_DATACALL_DEMO_DEFAULT_RECONNECT_MAX_COUNT,EASY_DATACALL_DEMO_DEFAULT_DATACALL_TIMEOUT}
};
qapp_easy_datacall_config_t g_easy_datacall_test_config2 =
{
    .pdp_config =   {EASY_DATACALL_DEMO_DEFAULT_SIM_ID,EASY_DATACALL_DEMO_PDP_ID_2,
                     QOSA_DATACALL_CONN_TCPIP,EASY_DATACALL_DEMO_DEFAULT_APN,QOSA_PDP_IPV4,
                     QOSA_PDP_AUTH_TYPE_NONE,EASY_DATACALL_DEMO_DEFAULT_UN,EASY_DATACALL_DEMO_DEFAULT_PW},
    .retry_config = {EASY_DATACALL_DEMO_DEFAULT_RECONNECT_MODE,EASY_DATACALL_DEMO_DEFAULT_REG_TIMEOUT,
                     EASY_DATACALL_DEMO_DEFAULT_RECONNECT_MAX_COUNT,EASY_DATACALL_DEMO_DEFAULT_DATACALL_TIMEOUT}
};
/*========================================================================
*  Func Definition
*========================================================================*/
void easy_datacall_demo_callback(qapp_easy_datacall_event_e event_id, void *ctx)
{
    qapp_easy_datacall_simid_pdpid_info_t *p_info = (qapp_easy_datacall_simid_pdpid_info_t*)ctx;
    QLOGI("simid %d  pdpid %d,easy datacall event:%d 0-succ 1-fail 2-disconnect 3-reconnect 4-nw_not_reg",p_info->simid,p_info->pdpid,event_id);
    return ;
}

static void easy_datacall_test_demo_task(void *arg)
{
    QOSA_UNUSED(arg);

    qosa_task_sleep_sec(3);
    QLOGI("enter easy_datacall_test_demo_task");
    qapp_easy_datacall_error_e ret = QAPP_EASY_DATACALL_OK;

    ret = qapp_easy_datacall(g_easy_datacall_test_config1,easy_datacall_demo_callback);
    QLOGI("ret = %d",ret);

    ret = qapp_easy_datacall(g_easy_datacall_test_config2,easy_datacall_demo_callback);
    QLOGI("ret = %d",ret);

    while (1)
    {
        qosa_task_sleep_sec(1);
    }
}

void unir_easy_datacall_demo_init()
{
    int err = 0;
    QLOGI("enter easy_datacall_demo_init");
    // Create easy datacall demo task
    err = qosa_task_create(&g_easy_datacall_test_task, 1024 * 4, QOSA_PRIORITY_NORMAL, "EZDTCDEMO", easy_datacall_test_demo_task, QOSA_NULL);
    if (err != QOSA_OK)
    {
        QLOGD("easy datacall demo create error");
        return;
    }
}
