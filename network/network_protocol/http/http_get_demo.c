#include "quecos_atcmd_cfg.h"
#include "quecos_http_app.h"
#include "qosa_def.h"
#include "qosa_sys.h"
#include "qosa_log.h"
#include "qurl.h"
#include "qosa_datacall.h"

#define QURL_HTTP_APP_MAX(a, b) (((a) > (b)) ? (a) : (b))
#define QURL_HTTP_APP_MIN(a, b) (((a) < (b)) ? (a) : (b))

//自定义请求头数据
static char  head_data[] = "GET / HTTP/1.1\r\nHost: www.baidu.com\r\nAccept: */*\r\n\r\n";
static char  g_buff[2049] = {0};
struct qurl_app_r_buf_s
{
    char *buf_ptr;
    long  buf_len;
    long  r_index;
};
typedef struct qurl_app_r_buf_s qurl_app_r_buf_t;

static qosa_int32_t license_http_datacall_active(void)
{
    /* Define data connection related variables */
    qosa_datacall_conn_t    conn = 0;    // Data connection object
    qosa_datacall_ip_info_t info = {0};  // IP information structure
    qosa_datacall_errno_e   ret = 0;     // Error code
    qosa_pdp_context_t      pdp_ctx = {0};
    qosa_uint8_t            simid = 0;
    int                     profile_idx = 1;
    qosa_bool_t             is_attached = QOSA_FALSE;

    is_attached = qosa_datacall_wait_attached(simid, DATACALL_DEMO_WAIT_ATTACH_MAX_WAIT_TIME);
    if (!is_attached)
    {
        QLOGI("attach fail");
    }
    const char *apn_str = "test";
    pdp_ctx.apn_valid = QOSA_TRUE;
    pdp_ctx.pdp_type = QOSA_PDP_TYPE_IP;  //ipv4
    if (pdp_ctx.apn_valid)
    {
        qosa_memcpy(pdp_ctx.apn, apn_str, qosa_strlen(apn_str));
    }
    ret = qosa_datacall_set_pdp_context(simid, profile_idx, &pdp_ctx);

    /* Create new data connection object */
    conn = qosa_datacall_conn_new(LICENSE_SET_SIM_CID, LICENSE_SET_PDP_CID, QOSA_DATACALL_CONN_TCPIP);
    /* Check data connection information to determine if PDP is activated */
    if (QOSA_DATACALL_ERR_NO_ACTIVE == qosa_datacall_get_ip_info(conn, &info))
    {
        QLOGD("SIM Card ID=%d,PDP ID=%d", LICENSE_SET_SIM_CID, LICENSE_SET_PDP_CID);
        /* If PDP is not activated, start synchronous activation process */
        ret = qosa_datacall_start(conn, LICENSE_HTTP_DEMO_ACTIVE_TIMEOUT);
        if (QOSA_DATACALL_OK != ret)
        {
            QLOGE("Data connection failed ret=%x", ret);
            return -1;
        }
    }
    return 0;
}

//用户读取请求 header 的回调函数，示例固定读取head_data，实际使用可以根据需要输入特定的自定义请求头
static long qurl_http_app_r_raw_head_cb(char *buf, long size, void *arg)
{
    qurl_app_r_buf_t *r_buf_ptr = (qurl_app_r_buf_t *)arg;

    if (size < 1)
    {
        return 0;
    }

    size = QURL_HTTP_APP_MIN(size, r_buf_ptr->buf_len - r_buf_ptr->r_index);
    memcpy(buf, r_buf_ptr->buf_ptr, size);
    r_buf_ptr->r_index += size;

    return size;
}
//用户接收回应 header 的回调函数，示例只是将回应 header 打印出来，实际使用可以根据需要进行特定的处理。
static long qurl_http_app_w_h_cb(char *buf, long size, void *arg)
{
    long len = size;
    long log_len = len > 2048 ? 2048 : len;

    (void)arg;

    memcpy(g_buff, buf, log_len);
    g_buff[log_len] = 0;

    QLOGV("%s", g_buff);

    return len;
}
//用户接收回应 body 的回调函数，示例只是将回应 body 打印出来，实际使用可以根据需要进行特定的处理。
static long qurl_http_app_w_cb(char *buf, long size, void *arg)
{
    long len = size;
    long log_len = len > 2048 ? 2048 : len;

    (void)arg;

    memcpy(g_buff, buf, log_len);
    g_buff[log_len] = 0;

    QLOGV("%s", g_buff);
    QLOGV("%d\r\n", size);

    return len;
}

/**
 * @brief 通过qurl实现http get功能演示
 *
 * 该函数实现了一个 http get 操作的完整流程，展示了如何基于qurl系统发起 http get 请求去获取http服务器上的资源。
 * 主要实现了 qurl 相关配置项的配置以及 qurl 请求的发起。
 * 包括一个新的 qurl 实例的创建用于发起 http get请求。
 * 配置 http get 请求的url。
 * 配置pdp通道，qurl传输操作总的超时时间以及http请求方法。
 * 配置用户读取请求 header 的回调函数以及传入回调函数中的用户参数。读取请求 header 的一种方式：通过回调函数去读取请求header。
 * 配置用户接收回应 body 的回调函数以及传入回调函数中的用户参数。
 * 配置 HTTP 身份验证方案以及登录服务器使用的用户名和密码。
 * 配置 http 请求使用自定义请求头。
 * 配置使能跟随重定向。
 * 配置 http get 请求的下载范围。
 * 配置用户接收回应 header 的回调函数以及传入回调函数中的用户参数。
 * qurl_core_perform发起http get请求。等待请求结果并通过回调函数处理http get过程中的数据。
 * 请求结束释放之前使用到的单向链表。
 * 请求结束删除之前创建的qurl实例。
 */
qosa_int32_t http_get_demo()
{
    qurl_core_t  core = QOSA_NULL;
    qurl_ecode_t     ret = QURL_OK;
    qurl_slist_t     qurl_headers = QOSA_NULL;
    qosa_bool_t      raw_request = 1;
    qurl_app_r_buf_t r_head_buf = {0};

    r_head_buf.buf_ptr = head_data;
    r_head_buf.buf_len = qosa_strlen(head_data);
    r_head_buf.r_index = 0;

    qosa_nw_err_e res = QOSA_NW_ERR_OK;  // Network error code
    qosa_uint8_t reg_status = {0};  // Network registration status
    qosa_uint8_t i = 0;             // Loop counter
// Check network registration status, retry up to 10 times
    for (i = 0; i < 10; i++)
    {
        res = qosa_nw_get_reg_status(LICENSE_SET_SIM_CID, QOSA_NULL, &reg_status);
        if ((res != QOSA_NW_ERR_OK) || (QOSA_FALSE == QOSA_NW_ATTACHED(reg_status)))
        {
            QLOGE("Network status error: 0x%x, Registration status:%d", res, reg_status);
            if (i >= 9)
            {
                QLOGE("Network registration failed!");
                return;
            }
            qosa_task_sleep_sec(1);  // Wait 1 second before retry
        }
        else
        {
            i = 0;
            QLOGV("Network registration successful!");
            break;
        }
    }

// Activate data connection
    license_http_datacall_active();
    if (QURL_OK != qurl_global_init()) {
    QLOGE("qurl_global_init failed");
    return;
    }
    QLOGD("qurl_global_init success");

    if (QURL_OK != qurl_core_create(&core)) {
        QLOGE("qurl_core_create failed");
        qurl_global_deinit();
        return;
    }

    QLOGD("qurl_core_create success");
    QLOGV("%p\r\n", core);
    //配置 http get请求的url
    qurl_core_setopt(core, QURL_OPT_URL, "http://www.baidu.com");
    //配置使用的pdp通道为1
    qurl_core_setopt(core, QURL_OPT_NETWORK_ID, 1);
    //配置qurl传输操作总的超时时间为30s
    qurl_core_setopt(core, QURL_OPT_TIMEOUT_MS, 30 * 1000);
    //配置http请求方法为GET请求
    qurl_core_setopt(core, QURL_OPT_HTTP_GET, 1L);
    //配置用户读取请求 header 的回调函数，读取请求 header 的一种方式，其他方法可以参考通用配置项中的QURL_OPT_UPLOAD_HEAD_DATA和QURL_OPT_UPLOAD_HEAD_FILE
    qurl_core_setopt(core, QURL_OPT_READ_HEAD_CB, qurl_http_app_r_raw_head_cb);
    //配置传入读取请求 header 回调函数中的用户参数，其中包含自定义请求头的内容
    qurl_core_setopt(core, QURL_OPT_READ_HEAD_CB_ARG, &r_head_buf);
    //配置用户接收回应 body 的回调函数
    qurl_core_setopt(core, QURL_OPT_WRITE_CB, qurl_http_app_w_cb);
    //配置传入接收回应 body 回调函数中的用户参数，用户可根据需要传入想要的参数
    //qurl_core_setopt(core, QURL_OPT_WRITE_CB_ARG, arg);
    if (raw_request == 0)
    {
        //配置 HTTP 身份验证方案以及登录服务器使用的用户名和密码
        qurl_core_setopt(core, QURL_OPT_HTTP_AUTH, QURL_HTTP_AUTH_BASIC);
        qurl_core_setopt(core, QURL_OPT_USERNAME, "test");
        qurl_core_setopt(core, QURL_OPT_PASSWORD, "test");
        //手动将请求头项插入到字符串链表中
        qurl_headers = qurl_slist_add_strdup(qurl_headers, "Content-Type: 05");
        qurl_headers = qurl_slist_add_strdup(qurl_headers, "Accept-Charset: ascii");
        qurl_core_setopt(core, QURL_OPT_HTTP_HEADER, qurl_headers);
    }
    else
    {
        //http 请求使用自定义请求头,通过回调函数读取自定义请求头
        qurl_core_setopt(core, QURL_OPT_UPLOAD_HEAD_RAW, 1L);
    }
    //使能跟随重定向
    qurl_core_setopt(core, QURL_OPT_FOLLOWLOCATION, 1L);

    //http get请求服务器资源的下载范围
    qurl_core_setopt(core, QURL_OPT_RANGE, "100-200");
    //配置用户接收回应 header 的回调函数
    qurl_core_setopt(core, QURL_OPT_WRITE_HEAD_CB, qurl_http_app_w_h_cb);
    //配置传入接收回应 header 回调函数中的用户参数，用户可根据需要传入想要的参数
    //qurl_core_setopt(core, QURL_OPT_WRITE_HEAD_CB_ARG, arg);

    //发起 http get 请求，阻塞等待请求的结果
    ret = qurl_core_perform(core);
     if (ret != QURL_OK)
     {
         QLOGE("%x\r\n", ret);
         goto exit;
     }
     //请求结束释放之前使用到的单向链表
     if (qurl_headers != QOSA_NULL)
     {
         qurl_slist_del_all(qurl_headers);
         qurl_headers = QOSA_NULL;
     }
     //请求结束删除之前创建的qurl实例
     qurl_core_delete(core);

exit:
    if (qurl_headers != QOSA_NULL)
    {
        qurl_slist_del_all(qurl_headers);
        qurl_headers = QOSA_NULL;
    }
    if (core != QOSA_NULL)
    {
        qurl_core_delete(core);
    }
    return;
}