// DNS_DEMO_TASK_STACK_SIZE 4096

// DNS 需要配置的参数
#define QUEC_DNS_DEMO_SIMID           0
#define QUEC_DNS_DEMO_PDPID           1
#define QUEC_DNS_DEMO_HOSTNAME        "www.baidu.com"

/**
 * @brief dns回调响应数据结构体
 *
 * 该结构体用于存储dns操作的响应信息，包括错误码、
 * 响应数据以及用户自定义参数
 */
typedef struct
{
    qosa_dns_error_e    evt_code;   /**< dns事件错误码 */
    struct qosa_addrinfo_s *info;   /**< dns响应数据指针 */
    void               *user_param; /**< 用户自定义参数指针 */
} dns_demo_resp_t;

static qosa_task_t g_dns_demo_task = QOSA_NULL;
static qosa_msgq_t g_dns_demo_msg = QOSA_NULL;

/**
 * @brief dns结果回调函数
 *
 * 该函数用于处理dns操作的结果回调，将dns响应数据进行封装并通过消息队列发送
 *
 * @param argv 用户自定义参数
 * @param info dns解析结果指针
 * @param type dns错误码
 */
static void quec_dns_result_cb(void *argv, struct qosa_addrinfo_s **info, qosa_dns_error_e type)
{
    dns_demo_resp_t dns_rsp = {0};

    dns_rsp.evt_code = type;
    dns_rsp.info = *info;  // 注意：这里复制指针，实际使用时需小心内存管理
    dns_rsp.user_param = argv;

    /* 发送消息 */
    qosa_msgq_release(g_dns_demo_msg, sizeof(dns_demo_resp_t), (qosa_uint8_t *)&dns_rsp, QOSA_NO_WAIT);
}

/**
 * @brief 处理DNS操作的结果回调函数，根据类型解析并记录DNS信息。
 *
 * 该函数处理来自DNS模块的响应事件，根据错误码处理解析结果，
 * 并将IP地址信息通过日志输出。处理完成后释放内存并标记完成。
 *
 * @param[in] dns_ptr 指向DNS响应数据结构的指针，包含错误码及响应内容
 * @return 返回是否处理完成的标志，QOSA_TRUE表示处理结束，QOSA_FALSE表示未结束
 */
static qosa_bool_t quec_dns_result_handler(dns_demo_resp_t *dns_ptr)
{
    qosa_int32_t evt_code = dns_ptr->evt_code;  // 提取事件码
    qosa_bool_t  finish = QOSA_FALSE;           // 是否完成标志，默认为未完成

    // 打印事件码，便于调试追踪
    quec_dns_log("evt_code=%x", evt_code);

    // 处理DNS解析结果
    if (evt_code == QOSA_DNS_RESULT_OK)
    {
        struct qosa_addrinfo_s *info = dns_ptr->info;
        if (info != QOSA_NULL)
        {
            while (info != QOSA_NULL)
            {
                // 打印IP地址和家族类型
                quec_dns_log("IP: %s, Family: %d", info->ip_addr, info->ai_family);
                info = info->ai_next;
            }
            // 释放内存
            qosa_dns_result_free(dns_ptr->info);
        }
    }
    else
    {
        quec_dns_log("DNS failed: %x", evt_code);
    }
    // 标记处理完成
    finish = QOSA_TRUE;


    // 返回处理完成标志
    return finish;
}

/**
 * @brief dns功能演示处理函数
 *
 * 该函数实现了一个dns操作的完整流程，包括初始化dns参数、启动dns操作、
 * 等待并处理dns结果等步骤。函数会在启动dns后进入循环等待状态，直到
 * dns操作完成或出现错误。
 */
static void quec_dns_demo_process(void *ctx)
{
    struct qosa_addrinfo_s hints = {0};
    dns_demo_resp_t        rsp = {0};
    qosa_dns_error_e       ret = 0;
    qosa_bool_t            is_finish = QOSA_FALSE;

    // 等待10秒，方便抓取log和网络就绪
    qosa_task_sleep_sec(10);

    // 配置hints，指定IPv4（AF_INET）或IPv6（AF_INET6）
    hints.ai_family = AF_INET;  // 示例使用IPv4

    // 启动dns异步操作，指定SIMID、PDPID、域名等参数
    ret = qosa_dns_asyn_getaddrinfo(QUEC_DNS_DEMO_SIMID, QUEC_DNS_DEMO_PDPID, QUEC_DNS_DEMO_HOSTNAME, &hints, quec_dns_result_cb, QOSA_NULL);
    if (ret != QOSA_DNS_RESULT_OK)
    {
        quec_dns_log("dns start err =%x", ret);
        qosa_msgq_delete(g_dns_demo_msg);
        return;
    }

    // 循环等待并处理dns结果消息
    while (1)
    {
        // 等待同步结果消息并进行处理
        qosa_msgq_wait(g_dns_demo_msg, (qosa_uint8_t *)&rsp, sizeof(dns_demo_resp_t), QOSA_WAIT_FOREVER);
        is_finish = quec_dns_result_handler(&rsp);
        if (is_finish)
        {
            // dns 处理结束
            break;
        }
    }
    qosa_msgq_delete(g_dns_demo_msg);
    quec_dns_log("DNS END");
}

void quec_demo_dns_init(void)
{
    quec_dns_log("enter Quectel DNS DEMO !!!");
    if (g_dns_demo_msg == QOSA_NULL)
    {
        qosa_msgq_create(&g_dns_demo_msg, sizeof(dns_demo_resp_t), 10);
    }
    if (g_dns_demo_task == QOSA_NULL)
    {
        qosa_task_create(&g_dns_demo_task, QUEC_DNS_DEMO_TASK_STACK_SIZE, QOSA_PRIORITY_NORMAL, "dns_demo", quec_dns_demo_process, QOSA_NULL);
    }
}