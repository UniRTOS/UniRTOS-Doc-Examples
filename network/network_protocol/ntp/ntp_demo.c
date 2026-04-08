#include "qosa_sys.h"
#include "qosa_def.h"
#include "qosa_log.h"
#include "qosa_rtc.h"
#include "qcm_ntp_app.h"

#define quec_ntp_log(...)             QOSA_LOG_D(LOG_TAG, ##__VA_ARGS__)

#define QUEC_NTP_DEMO_TASK_STACK_SIZE 4096

// NTP 服务器地址及端口
#define QUEC_NTP_DEMO_SERVER          "ntp.aliyun.com"
#define QUEC_NTP_DEMO_PORT            123

// NTP 需要配置的参数
#define QUEC_NTP_DEMO_SIMID           0
#define QUEC_NTP_DEMO_PDPID           1
// NTP 重试次数及重试间隔时间
#define QUEC_NTP_DEMO_RETRY_CNT       3
#define QUEC_NTP_DEMO_RETRY_TIMEOUT   15

/**
 * @struct qapp_ntp_msg_t
 * @brief ntp cb函数返回应答事件通知
 */
typedef struct
{
    qcm_ntp_client_id   client_id;  /*!< 对应当前在执行的NTP id */
    qcm_ntp_result_code result;     /*!< NTP APP执行结果返回 */
    qosa_rtc_time_t     sync_time;  /*!< 如果成功则返回NTP查询到的时间 */
    void               *user_param; /*!< 对应用户启动时携带的用户参数 */
} qapp_ntp_msg_t;


/*===========================================================================
 *  Variate
 ===========================================================================*/

static qosa_task_t g_ntp_demo_task = QOSA_NULL;
static qosa_msgq_t g_ntp_demo_msg = QOSA_NULL;

/*===========================================================================
 *  Static API Functions
 ===========================================================================*/

/**
 * NTP 时间查询同步结果回调函数
 *
 * 该函数用于处理NTP客户端时间查询完成后的结果回调
 * 并发送到消息队列中供其他模块处理
 *
 * @param client_id NTP客户端标识符
 * @param result NTP 查询结果码，表示查询成功或失败
 * @param sync_time 查询后的时间信息结构体指针
 * @param arg 用户自定义参数指针
 */
static void quec_ntp_sync_result_cb(qcm_ntp_client_id client_id, qcm_ntp_result_code result, qosa_rtc_time_t *sync_time, void *arg)
{
    qapp_ntp_msg_t msg = {0};

    /* 封装NTP查询结果消息 */
    msg.client_id = client_id;
    msg.result = result;
    msg.user_param = arg;
    qosa_memcpy(&msg.sync_time, sync_time, sizeof(qosa_rtc_time_t));

    /* 发送消息 */
    qosa_msgq_release(g_ntp_demo_msg, sizeof(qapp_ntp_msg_t), (qosa_uint8_t *)&msg, QOSA_NO_WAIT);
}

/**
 * @brief NTP 结果处理函数
 * @param msg NTP消息结构体指针，包含查询结果和时间信息
 */
static void quec_ntp_result_handler(qapp_ntp_msg_t *msg)
{
    QOSA_UNUSED(msg);
    char             rsp_ptr[QOSA_ARRAY_BYTE_128] = {0};  // 用于存储时间信息字符串
    qosa_int32_t     rsp_len = 0;                         // 用于记录时间信息字符串长度
    qosa_rtc_time_t *time = QOSA_NULL;                    // 用于存储查询后的时间信息
    qosa_int8_t      timezone = qosa_rtc_get_timezone();  // 用于获取当前时区

    /* 检查消息指针是否为空 */
    if (msg == QOSA_NULL)
    {
        return;
    }

    /* 根据NTP查询结果进行相应处理 */
    if (msg->result == QCM_NTP_SUCCESS)
    {
        /* 查询成功：格式化时间信息并记录日志 */
        time = &msg->sync_time;
        rsp_len += qosa_snprintf(
            rsp_ptr,
            QOSA_ARRAY_BYTE_128,
            "\"%04d/%02d/%02d,%02d:%02d:%02d%c%02d\"",
            time->tm_year + 1900,
            time->tm_mon + 1,
            time->tm_mday,
            time->tm_hour,
            time->tm_min,
            time->tm_sec,
            timezone >= 0 ? '+' : '-',
            QOSA_ABS(timezone)
        );
        quec_ntp_log("NTP_TIME: [%s]", rsp_ptr);
    }
    else
    {
        /* 查询失败：记录错误码 */
        quec_ntp_log("ntp errcode=%x", msg->result);
    }
}

/**
 * @brief NTP 查询演示处理函数
 * @param ctx 任务上下文指针，未使用
 *
 * 该函数演示了NTP时间查询同步的完整流程，包括创建NTP客户端、配置参数、
 * 启动异步同步以及处理同步结果等操作。
 */
static void quec_ntp_demo_process(void *ctx)
{
    qcm_ntp_client_id   client_id = 0;
    qcm_ntp_config_t    ntp_options = {0};
    qapp_ntp_msg_t      msg = {0};
    qcm_ntp_result_code ret = 0;

    // 等待10秒，方便抓取log和开机注网
    qosa_task_sleep_sec(10);

    // 申请NTP client id
    client_id = qcm_ntp_client_new();
    if (client_id <= 0)
    {
        quec_ntp_log("ntp err");
        return;
    }

    // 配置NTP参数并启动同步过程
    ntp_options.sim_id = QUEC_NTP_DEMO_SIMID;
    ntp_options.pdp_id = QUEC_NTP_DEMO_PDPID;

    ntp_options.num_data_bytes = 48;
    ntp_options.retry_cnt = QUEC_NTP_DEMO_RETRY_CNT;
    // 设置同步本地时间标志，此处不更新本地时间, 如果设置为 QOSA_TRUE 则会同步到本地时间
    ntp_options.sync_local_time = QOSA_FALSE;
    ntp_options.retry_interval_tm = QUEC_NTP_DEMO_RETRY_TIMEOUT;

    // 启动 NTP 异步执行
    ret = qcm_ntp_sync_start(client_id, QUEC_NTP_DEMO_SERVER, QUEC_NTP_DEMO_PORT, &ntp_options, quec_ntp_sync_result_cb, QOSA_NULL);
    if (ret != QCM_NTP_SUCCESS)
    {
        quec_ntp_log("ntp start err =%x", ret);
        qosa_msgq_delete(g_ntp_demo_msg);
        return;
    }

    // 等待同步结果消息并进行处理
    qosa_msgq_wait(g_ntp_demo_msg, (qosa_uint8_t *)&msg, sizeof(qapp_ntp_msg_t), QOSA_WAIT_FOREVER);
    quec_ntp_result_handler(&msg);
    qosa_msgq_delete(g_ntp_demo_msg);
}

void quec_demo_ntp_init(void)
{
    quec_ntp_log("enter Quectel NTP DEMO !!!");
    if (g_ntp_demo_msg == QOSA_NULL)
    {
        qosa_msgq_create(&g_ntp_demo_msg, sizeof(qapp_ntp_msg_t), 5);
    }
    if (g_ntp_demo_task == QOSA_NULL)
    {
        qosa_task_create(&g_ntp_demo_task, QUEC_NTP_DEMO_TASK_STACK_SIZE, QOSA_PRIORITY_NORMAL, "ntp_demo", quec_ntp_demo_process, QOSA_NULL);
    }
}