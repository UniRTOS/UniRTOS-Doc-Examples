#include "qosa_def.h"
#include "qosa_sys.h"
#include "qosa_lpm.h"
#include "qosa_log.h"
#include "qosa_event_notify.h"
#include "lpm_demo.h"

#define QOS_LOG_TAG LOG_TAG

// 事件定义
#define LPM_DEMO_EVENT_SLEEP_STATUS (QOSA_EVENT_LPM_SLEEP_STATUS + 100)

/** lpm demo task handle */
static qosa_task_t g_lpm_demo_task = QOSA_NULL;

/** lpm demo message queue handle */
static qosa_msgq_t g_lpm_demo_queue = QOSA_NULL;

/** 应用投票句柄 */
static qosa_handle g_app_vote_handle = QOSA_NULL;

// ===========================================================================
// [Data Structure] Define messages for communication between Callback and MainTask
// ===========================================================================

/**
 * @struct demo_lpm_msg_t
 * @brief LPM demo message structure for communication between callback and main task
 */
typedef struct
{
    qosa_int32_t event_id; /*!< Event type */
    union
    {
        qosa_lpm_sleep_event_t sleep_event; /*!< Sleep event data */
        qosa_lpm_psm_event_t psm_event;    /*!< PSM event data */
        qosa_int32_t result_code;           /*!< General result code */
    } data;                                /*!< Union data for different events */
} demo_lpm_msg_t;

// ===========================================================================
// [Producer] Callback functions
// Features: Run in underlying threads/interrupts, only responsible for packaging messages and sending them to the queue
// ===========================================================================

/**
 * @brief LPM 休眠事件回调函数
 * 
 * @param [in] user_argv
 *           - 用户自定义参数，本函数中未使用
 * @param [in] argv
 *           - 事件参数指针，指向 qosa_lpm_sleep_event_t 结构体
 * 
 * @return 返回 0 表示成功处理
 */
static int demo_lpm_sleep_callback(void *user_argv, void *argv)
{
    QOSA_UNUSED(user_argv);
    
    demo_lpm_msg_t         msg = {0};
    qosa_lpm_sleep_event_t *ev = (qosa_lpm_sleep_event_t *)argv;
    
    QLOGI("[LPM EVENT] Sleep Status: %d Reason: %d", ev->status, ev->reason);
    
    msg.event_id = LPM_DEMO_EVENT_SLEEP_STATUS;
    // 深拷贝事件数据
    qosa_memcpy(&msg.data.sleep_event, ev, sizeof(qosa_lpm_sleep_event_t));
    
    if (g_lpm_demo_queue != QOSA_NULL)
    {
        qosa_msgq_release(g_lpm_demo_queue, sizeof(msg), (qosa_uint8_t *)&msg, QOSA_NO_WAIT);
    }
    
    return 0;
}

// ===========================================================================
// [Business Logic] Specific functional implementation functions
// ===========================================================================

/**
 * @brief 配置 LPM 参数
 * 
 * @param None
 * 
 * @return qosa_lpm_error_e
 *         - 成功返回 QOSA_LPM_ERR_OK
 *         - 失败返回其他错误码
 */
static qosa_lpm_error_e business_config_lpm(void)
{
    qosa_lpm_config_t config = {0};
    
    // 获取当前配置
    qosa_lpm_config_get(&config);
    QLOGI("Current LPM Mode: %d", config.lpm_mode);
    
    // 配置休眠参数
    config.lpm_mode = QOSA_LPM_MODE_ENABLE;      // 启用休眠
    config.delay_time = 5000;                    // 延迟 5 秒进入休眠
    config.dtr_en = QOSA_LPM_DTR_ENABLE;          // 启用 DTR 唤醒
    config.uart_en = QOSA_LPM_UART_ENABLE;        // 启用 UART 唤醒
    config.rfr_enable = QOSA_TRUE;               // 启用 RRC 快速释放
    config.no_data_time = 10;                     // 10 秒无数据释放 RRC
    config.retry_time = 600;                      // 异常后 10 分钟重试
    
    // 配置 PSM 参数（可选）
    qosa_strncpy((char *)config.tau, "01000010", QOSA_LPM_PTAUS_MAX_LEN);
    qosa_strncpy((char *)config.active, "00100010", QOSA_LPM_PTAUS_MAX_LEN);
    
    qosa_lpm_error_e ret = qosa_lpm_config_set(&config);
    if (ret == QOSA_LPM_ERR_OK)
    {
        QLOGI("LPM Config Set Success");
    }
    else
    {
        QLOGE("LPM Config Set Failed: %d", ret);
    }
    
    return ret;
}

/**
 * @brief 创建应用投票句柄
 * 
 * @param None
 * 
 * @return qosa_lpm_error_e
 *         - 成功返回 QOSA_LPM_ERR_OK
 *         - 失败返回其他错误码
 */
static qosa_lpm_error_e business_create_vote_handle(void)
{
    if (g_app_vote_handle == QOSA_NULL)
    {
        qosa_lpm_error_e ret = qosa_lpm_app_vote_new_handle("lpm_demo", &g_app_vote_handle);
        if (ret == QOSA_LPM_ERR_OK)
        {
            QLOGI("Vote Handle Created Successfully");
        }
        else
        {
            QLOGE("Vote Handle Create Failed: %d", ret);
        }
        return ret;
    }
    return QOSA_LPM_ERR_OK;
}

/**
 * @brief 执行需要保持唤醒的任务
 * 
 * @param None
 * 
 * @return None
 */
static void business_do_wakeup_task(void)
{
    QLOGI("Starting Wakeup Task...");
    
    // 1. 禁止休眠
    qosa_lpm_error_e ret = qosa_lpm_app_vote_disable(g_app_vote_handle);
    if (ret != QOSA_LPM_ERR_OK)
    {
        QLOGE("Vote Disable Failed: %d", ret);
        return;
    }
    
    QLOGI("Device will stay awake now");
    
    // 2. 执行任务（例如：数据采集、网络传输等）
    qosa_task_sleep_ms(5000);  // 模拟任务执行 5 秒
    
    // 3. 任务完成，允许休眠
    ret = qosa_lpm_app_vote_enable(g_app_vote_handle);
    if (ret == QOSA_LPM_ERR_OK)
    {
        QLOGI("Vote Enabled, device can enter sleep now");
    }
    else
    {
        QLOGE("Vote Enable Failed: %d", ret);
    }
}

// ===========================================================================
// [Consumer] Demo main task
// Features: Has independent stack space, can safely handle complex logic, block waiting for messages
// ===========================================================================

/**
 * @brief LPM demo 任务主函数
 * 
 * @param [in] arg
 *           - 任务参数指针，本函数中未使用
 * 
 * @return 无返回值
 */
void lpm_demo_task_entry(void *arg)
{
    QOSA_UNUSED(arg);
    
    int              ret = 0;
    demo_lpm_msg_t    msg;
    
    QLOGI("Initializing LPM Demo...");
    
    // 1. 创建消息队列（深度 10，每条消息大小为 demo_lpm_msg_t）
    ret = qosa_msgq_create(&g_lpm_demo_queue, sizeof(demo_lpm_msg_t), 10);
    if (ret != QOSA_OK)
    {
        QLOGE("Create lpm demo queue failed: %d", ret);
        return;
    }
    
    // 2. 注册回调
    qosa_event_notify_register(QOSA_EVENT_LPM_SLEEP_STATUS, demo_lpm_sleep_callback, QOSA_NULL);
    
    // 3. 配置 LPM 参数
    business_config_lpm();
    
    // 4. 创建投票句柄
    business_create_vote_handle();
    
    // 5. 事件循环
    while (1)
    {
        // 阻塞等待消息
        if (qosa_msgq_wait(g_lpm_demo_queue, (qosa_uint8_t *)&msg, sizeof(msg), QOSA_WAIT_FOREVER) == 0)  // 0 == OK
        {
            switch (msg.event_id)
            {
                case LPM_DEMO_EVENT_SLEEP_STATUS: {
                    qosa_lpm_sleep_event_t *ev = &msg.data.sleep_event;
                    
                    if (ev->status == QOSA_LPM_SLEEP_STATUS_SLEEP)
                    {
                        QLOGI(">>> Device Entering Sleep, Reason: %d <<<", ev->reason);
                        // 设备进入休眠，可以保存状态、关闭外设等
                    }
                    else if (ev->status == QOSA_LPM_SLEEP_STATUS_WAKEUP)
                    {
                        QLOGI("<<< Device Waking Up, Reason: %d >>>", ev->reason);
                        // 设备唤醒，可以恢复状态、重新初始化外设等
                        
                        // 根据唤醒原因执行不同操作
                        switch (ev->reason)
                        {
                            case QOSA_LPM_WAKEUP_REASON_NET_DATA:
                                QLOGI("Wakeup by Network Data");
                                break;
                            case QOSA_LPM_WAKEUP_REASON_UART:
                                QLOGI("Wakeup by UART");
                                break;
                            case QOSA_LPM_WAKEUP_REASON_DTR:
                                QLOGI("Wakeup by DTR");
                                break;
                            case QOSA_LPM_WAKEUP_REASON_USB:
                                QLOGI("Wakeup by USB");
                                break;
                            default:
                                QLOGI("Wakeup by Other Reason");
                                break;
                        }
                    }
                    break;
                }
                default:
                    QLOGI("Event: Unhandled event ID %d", msg.event_id);
                    break;
            }
        }
        
        // 定期执行需要保持唤醒的任务（示例：每 60 秒）
        static qosa_uint32_t last_task_time = 0;
        if (qosa_sys_get_tick() - last_task_time > 60000)
        {
            business_do_wakeup_task();
            last_task_time = qosa_sys_get_tick();
        }
    }
}

/**
 * @brief 初始化 LPM demo
 * 
 * @param None
 * 
 * @return 无返回值
 * 
 * @note 任务栈大小为 4KB，优先级为低优先级
 */
void unir_lpm_demo_init(void)
{
    if (g_lpm_demo_task == QOSA_NULL)
    {
        int err = qosa_task_create(&g_lpm_demo_task, 4 * 1024, QOSA_PRIORITY_LOW, "lpm_demo", lpm_demo_task_entry, QOSA_NULL);
        if (err != QOSA_OK)
        {
            QLOGE("lpm_demo_init task create error");
            return;
        }
    }
}