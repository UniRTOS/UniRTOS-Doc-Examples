#include "qosa_sys.h"
#include "qosa_def.h"
#include "qosa_log.h"
#include "qcm_ping_app.h"
#define quec_ping_log(...)             QOSA_LOG_D(LOG_TAG, ##__VA_ARGS__)
#define QUEC_PING_DEMO_TASK_STACK_SIZE 4096

// PING 服务器地址
#define QUEC_PING_DEMO_SERVER          "www.baidu.com"

// PING 需要配置的参数
#define QUEC_PING_DEMO_SIMID           0
#define QUEC_PING_DEMO_PDPID           1
// PING 次数
#define QUEC_PING_DEMO_CNT             4
// PING 单次超时时间
#define QUEC_PING_DEMO_TIMEOUT         4
// PING TTL
#define QUEC_PING_DEMO_TTL             255

/**
 * @brief ping回调响应数据结构体
 *
 * 该结构体用于存储ping操作的响应信息，包括事件类型、错误码、
 * 响应数据以及用户自定义参数
 */
typedef struct
{
    qcm_ping_event_type event_id;   /**< ping事件类型 */
    qcm_ping_error_e    evt_code;   /**< ping事件错误码 */
    qcm_ping_resp_t     resp_ptr;   /**< ping响应数据指针 */
    void               *user_param; /**< 用户自定义参数指针 */
} ping_demo_resp_t;

static qosa_task_t g_ping_demo_task = QOSA_NULL;
static qosa_msgq_t g_ping_demo_msg = QOSA_NULL;
/**
 * @brief ping结果回调函数
 *
 * 该函数用于处理ping操作的结果回调，将ping响应数据进行封装并通过消息队列发送
 *
 * @param event_id ping事件类型
 * @param evt_code ping错误码
 * @param resp_ptr ping响应数据指针
 * @param user_param 用户自定义参数指针
 */
static void quec_ping_result_cb(qcm_ping_event_type event_id, qcm_ping_error_e evt_code, qcm_ping_resp_t *resp_ptr, void *user_param)
{
    ping_demo_resp_t ping_rsp = {0};

    ping_rsp.event_id = event_id;
    ping_rsp.evt_code = evt_code;
    ping_rsp.user_param = user_param;

    /* 根据事件类型复制相应的响应数据 */
    if (ping_rsp.event_id == QCM_PING_STATS)
    {
        qosa_memcpy(&ping_rsp.resp_ptr.type.status, &resp_ptr->type.status, sizeof(qcm_ping_stats_type));
    }
    else
    {
        qosa_memcpy(&ping_rsp.resp_ptr.type.summary, &resp_ptr->type.summary, sizeof(qcm_ping_summary_type));
    }

    /* 发送消息 */
    qosa_msgq_release(g_ping_demo_msg, sizeof(ping_demo_resp_t), (qosa_uint8_t *)&ping_rsp, QOSA_NO_WAIT);
}

/**
 * @brief 处理PING操作的结果回调函数，根据事件类型解析并记录PING统计或汇总信息。
 *
 * 该函数处理来自PING模块的响应事件，包括PING状态更新和最终的汇总信息。
 * 根据事件ID区分是单次PING结果(QCM_PING_STATS)还是最终汇总(QCM_PING_SUMMARY)，
 * 并将相关信息格式化后通过日志输出。当收到汇总事件时，标记处理完成。
 *
 * @param[in] ping_ptr 指向PING响应数据结构的指针，包含事件ID、事件码及响应内容
 * @return 返回是否处理完成的标志，QOSA_TRUE表示处理结束，QOSA_FALSE表示未结束
 */
static qosa_bool_t quec_ping_result_handler(ping_demo_resp_t *ping_ptr)
{
    char         resp_buf[256] = {0};            // 用于存储格式化后的响应字符串
    qosa_int32_t resp_len = 0;                   // 响应字符串长度（当前未实际使用）
    qosa_int32_t evt_code = ping_ptr->evt_code;  // 提取事件码
    qosa_bool_t  finish = QOSA_FALSE;            // 是否完成标志，默认为未完成

    // 打印事件ID和事件码，便于调试追踪
    quec_ping_log("event_id=%d,%x", ping_ptr->event_id, evt_code);

    // 根据不同的事件ID进行处理
    switch (ping_ptr->event_id)
    {
        // 处理单次PING的统计信息事件
        case QCM_PING_STATS: {
            // 只有在事件码为成功的情况下才处理
            if (evt_code == QCM_PING_OK)
            {
                // 获取PING状态信息结构体指针
                qcm_ping_stats_type *stats = &ping_ptr->resp_ptr.type.status;
                if (stats != QOSA_NULL)
                {
                    // 格式化PING状态信息
                    resp_len += qosa_snprintf(resp_buf, 256, "\"%s\",%ld,%ld,%ld", stats->resolved_ip_addr, stats->ping_size, stats->ping_rtt, stats->ping_ttl);
                    // 打印PING状态信息
                    quec_ping_log("PING: [%s]", resp_buf);
                }
            }
        }
        break;
        // 处理PING结束时的汇总信息事件
        case QCM_PING_SUMMARY: {
            // 只有在事件码为成功的情况下才处理
            if (evt_code == QCM_PING_OK)
            {
                // 获取PING汇总信息结构体指针
                qcm_ping_summary_type *summary = &ping_ptr->resp_ptr.type.summary;
                if (summary != QOSA_NULL)
                {
                    // 格式化PING汇总信息
                    resp_len += qosa_snprintf(
                        resp_buf,
                        256,
                        "%ld,%ld,%ld,%ld,%ld,%ld",
                        summary->num_pkts_sent,
                        summary->num_pkts_recvd,
                        summary->num_pkts_lost,
                        summary->min_rtt,
                        summary->max_rtt,
                        summary->avg_rtt
                    );
                    // 打印PING结束的汇总信息
                    quec_ping_log("PING_END: [%s]", resp_buf);
                }
            }
            // 标记处理完成
            finish = QOSA_TRUE;
        }
        break;
        default:
            break;
    }
    // 返回处理完成标志
    return finish;
}


/**
 * @brief ping功能演示处理函数
 *
 * 该函数实现了一个ping操作的完整流程，包括初始化ping参数、启动ping操作、
 * 等待并处理ping结果等步骤。函数会在启动ping后进入循环等待状态，直到
 * ping操作完成或出现错误。
 */
static void quec_ping_demo_process(void *ctx)
{
    qcm_ping_config_type ping_options = {0};
    ping_demo_resp_t     rsp = {0};
    qcm_ping_error_e     ret = 0;
    qosa_bool_t          is_finish = QOSA_FALSE;

    // 等待10秒，方便抓取log和开机注网
    qosa_task_sleep_sec(10);

    // 对进行ping操作的数据大小，次数，超时时间赋值，如果没有参数，默认值
    ping_options.num_data_bytes = 64;
    ping_options.num_pings = QUEC_PING_DEMO_CNT;
    ping_options.ping_response_time_out = QUEC_PING_DEMO_TIMEOUT;
    ping_options.ttl = QUEC_PING_DEMO_TTL;

    // 启动ping操作，指定PDPID、SIM卡ID、服务器地址等参数
    ret = qcm_ping_start(QUEC_PING_DEMO_PDPID, QUEC_PING_DEMO_SIMID, QUEC_PING_DEMO_SERVER, &ping_options, quec_ping_result_cb, QOSA_NULL);
    if (ret != QCM_PING_OK)
    {
        quec_ping_log("ping start err =%x", ret);
        qosa_msgq_delete(g_ping_demo_msg);
        return;
    }

    // 循环等待并处理ping结果消息
    while (1)
    {
        // 等待同步结果消息并进行处理
        qosa_msgq_wait(g_ping_demo_msg, (qosa_uint8_t *)&rsp, sizeof(ping_demo_resp_t), QOSA_WAIT_FOREVER);
        is_finish = quec_ping_result_handler(&rsp);
        if (is_finish)
        {
            // ping 处理结束
            break;
        }
    }
    qosa_msgq_delete(g_ping_demo_msg);
    quec_ping_log("PING END");
}

void quec_demo_ping_init(void)
{
    quec_ping_log("enter Quectel PING DEMO !!!");
    if (g_ping_demo_msg == QOSA_NULL)
    {
        qosa_msgq_create(&g_ping_demo_msg, sizeof(ping_demo_resp_t), 10);
    }
    if (g_ping_demo_task == QOSA_NULL)
    {
        qosa_task_create(&g_ping_demo_task, QUEC_PING_DEMO_TASK_STACK_SIZE, QOSA_PRIORITY_NORMAL, "ping_demo", quec_ping_demo_process, QOSA_NULL);
    }
}