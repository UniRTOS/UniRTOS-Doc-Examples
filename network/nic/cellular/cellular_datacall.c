#include "qosa_def.h"
#include "qosa_log.h"

#include "qosa_network.h"
#include "qosa_datacall.h"
#include "qosa_platform_cfg.h"
#include "qosa_ip_addr.h"
#include "qosa_event_notify.h"

#define log_e(...)                                QOSA_LOG_E(LOG_TAG_MODEM, ##__VA_ARGS__)
#define log_w(...)                                QOSA_LOG_W(LOG_TAG_MODEM, ##__VA_ARGS__)
#define log_i(...)                                QOSA_LOG_I(LOG_TAG_MODEM, ##__VA_ARGS__)
#define log_d(...)                                QOSA_LOG_D(LOG_TAG_MODEM, ##__VA_ARGS__)
#define log_v(...)                                QOSA_LOG_V(LOG_TAG_MODEM, ##__VA_ARGS__)

#define DATACALL_DEMO_WAIT_ATTACH_MAX_WAIT_TIME   300
#define DATACALL_DEMO_WAIT_DATACALL_MAX_WAIT_TIME 120

qosa_task_t g_datacall_demo_task = QOSA_NULL;
qosa_msgq_t g_datacall_demo_msgq = QOSA_NULL;

typedef enum
{
    DATACALL_NW_DEACT_MSG,
} datacall_demo_msg_e;

typedef struct
{
    datacall_demo_msg_e msgid; /**< datacall demo message type */
    void               *argv;
} datacall_demo_msg_t;

typedef struct
{
    qosa_uint8_t simid; /**< SIM identification */
    qosa_uint8_t pdpid; /**< PDP identification */
} datacall_demo_pdp_deact_ind_t;

int datacall_nw_deact_pdp_cb(void *user_argv, void *argv)
{
    QOSA_UNUSED(user_argv);

    datacall_demo_pdp_deact_ind_t *deact_ptr = QOSA_NULL;
    datacall_demo_msg_t            datacall_nw_deact_msg = {0};

    //get nw deact report params
    qosa_datacall_nw_deact_event_t *pdp_deatch_event = (qosa_datacall_nw_deact_event_t *)argv;

    log_i("enter,simid=%d,pdpid=%d", pdp_deatch_event->simid, pdp_deatch_event->pdpid);

    // melloc memory
    deact_ptr = (datacall_demo_pdp_deact_ind_t *)qosa_malloc(sizeof(datacall_demo_pdp_deact_ind_t));
    if (deact_ptr == QOSA_NULL)  // if melloc fail ,return
    {
        return 0;
    }

    deact_ptr->simid = pdp_deatch_event->simid;
    deact_ptr->pdpid = pdp_deatch_event->pdpid;

    // Preparing to send messages to the message queue
    datacall_nw_deact_msg.msgid = DATACALL_NW_DEACT_MSG;
    datacall_nw_deact_msg.argv = deact_ptr;

    qosa_msgq_release(g_datacall_demo_msgq, sizeof(datacall_demo_msg_t), (qosa_uint8_t *)&datacall_nw_deact_msg, QOSA_NO_WAIT);
    return 0;
}

static void datacall_demo_task(void *arg)
{
    int                     ret = 0;
    int                     retry_count = 0;
    uint8_t                 simid = 0;
    int                     profile_idx = 1;
    qosa_datacall_conn_t    conn;
    qosa_bool_t             datacall_status = QOSA_FALSE;
    qosa_datacall_ip_info_t info = {0};
    datacall_demo_msg_t     datacall_task_msg = {0};
    qosa_pdp_context_t      pdp_ctx = {0};
    qosa_bool_t             is_attached = QOSA_FALSE;
    char                    ip4addr_buf[CONFIG_QOSA_INET_ADDRSTRLEN] = {0};
    char                    ip6addr_buf[CONFIG_QOSA_INET6_ADDRSTRLEN] = {0};

    //create message queue
    ret = qosa_msgq_create(&g_datacall_demo_msgq, sizeof(datacall_demo_msg_t), 20);
    log_i("create msgq result=%d", ret);

    qosa_task_sleep_sec(3);

    //if attach is successful before the maxtime timeout, it will immediately return QOSA_TRUE and enter second while loop
    //Otherwise, it will block until timeout and returning QOSA_FALSE and enter first while loop
    is_attached = qosa_datacall_wait_attached(simid, DATACALL_DEMO_WAIT_ATTACH_MAX_WAIT_TIME);
    if (!is_attached)
    {
        log_i("attach fail");
        goto exit;
    }

    //register network pdn deactive event callback
    qosa_event_notify_register(QOSA_EVENT_NW_PDN_DEACT, datacall_nw_deact_pdp_cb, QOSA_NULL);

    //config pdp context:APN, iptype
    //If the operator has restrictions on the APN during registration, needs to be set the APN provided by the operator
    const char *apn_str = "test";
    pdp_ctx.apn_valid = QOSA_TRUE;
    pdp_ctx.pdp_type = QOSA_PDP_TYPE_IP;  //ipv4
    if (pdp_ctx.apn_valid)
    {
        qosa_memcpy(pdp_ctx.apn, apn_str, qosa_strlen(apn_str));
    }

    ret = qosa_datacall_set_pdp_context(simid, profile_idx, &pdp_ctx);
    log_i("set pdp context, ret=%d", ret);

    //create datacall object
    conn = qosa_datacall_conn_new(simid, profile_idx, QOSA_DATACALL_CONN_TCPIP);

    //start excute datacall(sync)
    ret = qosa_datacall_start(conn, DATACALL_DEMO_WAIT_DATACALL_MAX_WAIT_TIME);
    if (ret != QOSA_DATACALL_OK)
    {
        log_i("datacall fail ,ret=%d", ret);
        goto exit;
    }

    //get datacall status(0: deactive 1:active)
    datacall_status = qosa_datacall_get_status(conn);
    log_i("datacall status=%d", datacall_status);

    //get ip info from datacall
    ret = qosa_datacall_get_ip_info(conn, &info);
    log_i("pdpid=%d,simid=%d", info.simcid.pdpid, info.simcid.simid);
    log_i("ip_type=%d", info.ip_type);

    if (info.ip_type == QOSA_PDP_IPV4)
    {
        //ipv4 info
        qosa_memset(ip4addr_buf, 0, sizeof(ip4addr_buf));
        qosa_ip_addr_inet_ntop(QOSA_IP_ADDR_AF_INET, &info.ipv4_ip.addr.ipv4_addr, ip4addr_buf, sizeof(ip4addr_buf));
        log_i("ipv4 addr:%s", ip4addr_buf);
    }
    else if (info.ip_type == QOSA_PDP_IPV6)
    {
        //ipv6 info
        qosa_memset(ip6addr_buf, 0, sizeof(ip6addr_buf));
        qosa_ip_addr_inet_ntop(QOSA_IP_ADDR_AF_INET6, &info.ipv6_ip.addr.ipv6_addr, ip6addr_buf, sizeof(ip6addr_buf));
        log_i("ipv6 addr:%s", ip6addr_buf);
    }
    else
    {
        //ipv4 and ipv6 info
        qosa_memset(ip4addr_buf, 0, sizeof(ip4addr_buf));
        qosa_ip_addr_inet_ntop(QOSA_IP_ADDR_AF_INET, &info.ipv4_ip.addr.ipv4_addr, ip4addr_buf, sizeof(ip4addr_buf));
        log_i("ipv4 addr:%s", ip4addr_buf);
        qosa_memset(ip6addr_buf, 0, sizeof(ip6addr_buf));
        qosa_ip_addr_inet_ntop(QOSA_IP_ADDR_AF_INET6, &info.ipv6_ip.addr.ipv6_addr, ip6addr_buf, sizeof(ip6addr_buf));
        log_i("ipv6 addr:%s", ip6addr_buf);
    }

    while (1)
    {
        ret = qosa_msgq_wait(g_datacall_demo_msgq, (qosa_uint8_t *)&datacall_task_msg, sizeof(datacall_demo_msg_t), QOSA_WAIT_FOREVER);
        if (ret != 0)
            continue;
        log_i("enter datacall demo task, msgid=%d", datacall_task_msg.msgid);

        switch (datacall_task_msg.msgid)
        {
            case DATACALL_NW_DEACT_MSG: {
                qosa_datacall_nw_deact_event_t *pdp_deatch_event = (qosa_datacall_nw_deact_event_t *)datacall_task_msg.argv;
                log_i("simid=%d,deact pdpid=%d", pdp_deatch_event->simid, pdp_deatch_event->pdpid);

                //Try reactive 10 times, time interval is 20 seconds
                while (((ret = qosa_datacall_start(conn, DATACALL_DEMO_WAIT_DATACALL_MAX_WAIT_TIME)) != QOSA_DATACALL_OK) && (retry_count < 10))
                {
                    retry_count++;
                    log_i("datacall fail, the retry count is %d", retry_count);
                    qosa_task_sleep_sec(20);
                }

                if (ret == QOSA_DATACALL_OK)
                {
                    retry_count = 0;

                    //get datacall status(0: deactive 1:active)
                    datacall_status = qosa_datacall_get_status(conn);
                    log_i("datacall status=%d", datacall_status);
                    //get ip info from datacall
                    ret = qosa_datacall_get_ip_info(conn, &info);
                    log_i("pdpid=%d,simid=%d", info.simcid.pdpid, info.simcid.simid);
                    log_i("ip type=%d", info.ip_type);

                    char ip4addr_buf[CONFIG_QOSA_INET_ADDRSTRLEN] = {0};
                    char ip6addr_buf[CONFIG_QOSA_INET6_ADDRSTRLEN] = {0};

                    if (info.ip_type == QOSA_PDP_IPV4)
                    {
                        qosa_memset(ip4addr_buf, 0, sizeof(ip4addr_buf));
                        qosa_ip_addr_inet_ntop(QOSA_IP_ADDR_AF_INET, &info.ipv4_ip.addr.ipv4_addr, ip4addr_buf, sizeof(ip4addr_buf));
                        log_i("ipv4 addr:%s", ip4addr_buf);
                    }
                    else if (info.ip_type == QOSA_PDP_IPV6)
                    {
                        qosa_memset(ip6addr_buf, 0, sizeof(ip6addr_buf));
                        qosa_ip_addr_inet_ntop(QOSA_IP_ADDR_AF_INET6, &info.ipv6_ip.addr.ipv6_addr, ip6addr_buf, sizeof(ip6addr_buf));
                        log_i("ipv6 addr:%s", ip6addr_buf);
                    }
                    else
                    {
                        qosa_memset(ip4addr_buf, 0, sizeof(ip4addr_buf));
                        qosa_ip_addr_inet_ntop(QOSA_IP_ADDR_AF_INET, &info.ipv4_ip.addr.ipv4_addr, ip4addr_buf, sizeof(ip4addr_buf));
                        log_i("ipv4 addr:%s", ip4addr_buf);
                        qosa_memset(ip6addr_buf, 0, sizeof(ip6addr_buf));
                        qosa_ip_addr_inet_ntop(QOSA_IP_ADDR_AF_INET6, &info.ipv6_ip.addr.ipv6_addr, ip6addr_buf, sizeof(ip6addr_buf));
                        log_i("ipv6 addr:%s", ip6addr_buf);
                    }
                }
                else
                {
                    log_i("datacall fail in nw deact pdn event");
                }
                qosa_free(datacall_task_msg.argv);
            }
            break;

            default:
                break;
        }
    }

exit:
    //unregister network pdn deactive event callback
    qosa_event_notify_unregister(QOSA_EVENT_NW_PDN_DEACT, datacall_nw_deact_pdp_cb);

    //delete msgqueue and task
    qosa_msgq_delete(g_datacall_demo_msgq);
    qosa_task_delete(g_datacall_demo_task);
}

void datacall_demo_init(void)
{
    int err = 0;
    //create datacall demo main task
    err = qosa_task_create(&g_datacall_demo_task, 4 * 1024, QOSA_PRIORITY_NORMAL, "QDATACALLDEMO", datacall_demo_task, QOSA_NULL);

    if (err != QOSA_OK)
    {
        log_d("datacall_demo task create error");
        return;
    }
}