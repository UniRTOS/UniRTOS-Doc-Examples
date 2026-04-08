#include "quecos_atcmd_cfg.h"
#include "quecos_http_app.h"
#include "qosa_def.h"
#include "qosa_sys.h"
#include "qosa_log.h"
#include "qurl.h"

//自定义请求头数据
static char  head_data[] = "PUT / HTTP/1.1\r\nHost: 220.180.239.212:8300/processorder.php\r\nAccept: */*\r\n\r\n";
static char  body_data[] = "Message=1111&Appleqty=2222&Orangeqty=3333&find=1\r\n\r\n";
static char g_buff[2049] = {0};
struct qurl_app_r_buf_s
{
    char *buf_ptr;
    long  buf_len;
    long  r_index;
};
typedef struct qurl_app_r_buf_s qurl_app_r_buf_t;

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
//用户读取请求 body 的回调函数，示例固定读取body_data，实际使用可以根据需要输入特定的请求body
static long qurl_http_app_r_body_cb(char *buf, long size, void *arg)
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
 * @brief 通过 qurl 实现 http put 功能演示
 *
 * 该函数实现了一个 http put 操作的完整流程，展示了如何基于 qurl 系统发起 http put 请求向服务器上传数据并替换指定资源。
 * 主要实现了 qurl 相关配置项的配置以及 qurl 请求的发起。
 * 包括一个新的 qurl 实例的创建用于发起 http put请求。
 * 配置 http put 请求的url。
 * 配置pdp通道，qurl传输操作总的超时时间以及http请求方法。
 * 配置用户读取请求 header 的回调函数以及传入回调函数中的用户参数以及header的长度。读取请求 header 的一种方式：通过回调函数去读取请求header。
 * 配置用户读取请求 body 的回调函数以及传入回调函数中的用户参数以及body的长度。读取请求 body 的一种方式：通过回调函数去读取请求body。
 * 配置用户接收回应 body 的回调函数以及传入回调函数中的用户参数。
 * 配置 HTTP 身份验证方案以及登录服务器使用的用户名和密码。
 * 配置 http 请求使用自定义请求头。
 * 配置使能跟随重定向。
 * 配置 http put 请求的下载范围。
 * 配置用户接收回应 header 的回调函数以及传入回调函数中的用户参数。
 * qurl_core_perform发起 http put 请求。等待请求结果并通过回调函数处理 http put 过程中的数据。
 * 请求结束释放之前使用到的单向链表。
 * 请求结束删除之前创建的 qurl 实例。
 */
static qosa_int32_t http_put_demo(qurl_core_t http_core)
{
    qosa_int32_t    err = QOSA_OK;
    qurl_ecode_t    ret = QURL_OK;
    qurl_slist_t    qurl_headers = QOSA_NULL;
    int g_http_last_close_event = 0;
    qosa_bool_t raw_request = 1;
    qurl_app_r_buf_t r_head_buf = {0};
    qurl_app_r_buf_t r_body_buf = {0};

    r_head_buf.buf_ptr = head_data;
    r_head_buf.buf_len = qosa_strlen(head_data);
    r_head_buf.r_index = 0;

    r_body_buf.buf_ptr = body_data;
    r_body_buf.buf_len = qosa_strlen(body_data);
    r_body_buf.r_index = 0;

    qurl_global_init();

    if (http_core  == QOSA_NULL)
    {
        //没有可用的 qurl 实例，创建一个新的 qurl 实例用于发起 http put 请求。
        ret = qurl_core_create(&http_core);
        if (ret != QURL_OK)
        {
            err = OSA_HTTP_ERR_FAILURE;
            goto exit;
        }
    }

    QLOGV("%p\r\n", http_core);
    //配置 http put请求的url
    qurl_core_setopt(http_core, QURL_OPT_URL, "http://101.37.104.185:45410/1.txt");
    //配置使用的pdp通道为1
    qurl_core_setopt(http_core, QURL_OPT_NETWORK_ID, 1L);
    //配置qurl传输操作总的超时时间为30s
    qurl_core_setopt(http_core, QURL_OPT_TIMEOUT_MS, 30 * 1000);
    //配置http请求方法为 PUT 请求
    qurl_core_setopt(http_core, QURL_OPT_HTTP_PUT, 1L);
    //配置请求 header 的长度
    qurl_core_setopt(http_core, QURL_OPT_UPLOAD_HEAD_SIZE, qosa_strlen(head_data));
    //配置用户读取请求 header 的回调函数，读取请求 header 的一种方式，其他方法可以参考通用配置项中的QURL_OPT_UPLOAD_HEAD_DATA和QURL_OPT_UPLOAD_HEAD_FILE
    qurl_core_setopt(http_core, QURL_OPT_READ_HEAD_CB, qurl_http_app_r_raw_head_cb);
    //配置传入读取请求 header 回调函数中的用户参数，其中包含自定义请求头的内容
    qurl_core_setopt(http_core, QURL_OPT_READ_HEAD_CB_ARG, &r_body_buf);
    //配置请求 body 的长度
    qurl_core_setopt(http_core, QURL_OPT_UPLOAD_SIZE, qosa_strlen(body_data));
    //配置用户读取请求 body 的回调函数，读取请求 body 的一种方式，其他方法可以参考通用配置项中的QURL_OPT_UPLOAD_DATA和QURL_OPT_UPLOAD_FILE
    qurl_core_setopt(http_core, QURL_OPT_READ_CB, qurl_http_app_r_body_cb);
    //配置传入读取请求 body 回调函数中的用户参数，其中包含我们想要put的body的内容
    qurl_core_setopt(http_core, QURL_OPT_READ_CB_ARG, &r_head_buf);
    //配置用户接收回应 body 的回调函数
    qurl_core_setopt(http_core, QURL_OPT_WRITE_CB, qurl_http_app_w_cb);
    //配置传入接收回应 body 回调函数中的用户参数，用户可根据需要传入想要的参数
    //qurl_core_setopt(http_core, QURL_OPT_WRITE_CB_ARG, arg);

    if (raw_request == 0)
    {
        //配置 HTTP 身份验证方案以及登录服务器使用的用户名和密码
        qurl_core_setopt(http_core, QURL_OPT_HTTP_AUTH, QURL_HTTP_AUTH_BASIC);
        qurl_core_setopt(http_core, QURL_OPT_USERNAME, "test");
        qurl_core_setopt(http_core, QURL_OPT_PASSWORD, "test");
        //手动将请求头项插入到字符串链表中
        qurl_headers = qurl_slist_add_strdup(headers, "Content-Type: 05");
        qurl_headers = qurl_slist_add_strdup(headers, "Accept-Charset: ascii");
        qurl_core_setopt(http_core, QURL_OPT_HTTP_HEADER, qurl_headers);
    }
    else
    {
        //http 请求使用自定义请求头,通过回调函数读取自定义请求头
        qurl_core_setopt(http_core, QURL_OPT_UPLOAD_HEAD_RAW, 1L);
    }

    //使能跟随重定向
    qurl_core_setopt(http_core, QURL_OPT_FOLLOWLOCATION, 1L);

    //非标准用法，http put指定分块上传的范围，或者断点续传中指定从哪个字节位置继续上传文件
    qurl_core_setopt(http_core, QURL_OPT_RANGE, "100-200");
    //配置用户接收回应 header 的回调函数
    qurl_core_setopt(http_core, QURL_OPT_WRITE_HEAD_CB, http_write_head_cb);
    //配置传入接收回应 header 回调函数中的用户参数，用户可根据需要传入想要的参数
    //qurl_core_setopt(http_core, QURL_OPT_WRITE_HEAD_CB_ARG, http_write_head_cb_arg);

    //发起 http put 请求，阻塞等待请求的结果
    ret = qurl_core_perform(http_core);
    //获取 qurl 实例最后关闭的原因
    g_http_last_close_event = qurl_core_get_last_close_event(http_core);

    if (ret != QURL_OK)
    {
        QLOGE("%x\r\n", ret);
        err = OSA_HTTP_ERR_FAILURE;
        goto exit;
    }
    //请求结束释放之前使用到的单向链表
    if (qurl_headers != QOSA_NULL)
    {
        qurl_slist_del_all(qurl_headers);
        qurl_headers = QOSA_NULL;
    }
    //请求结束删除之前创建的qurl实例
    qurl_core_delete(http_core);

    return QOSA_OK;
exit:
    if (qurl_headers != QOSA_NULL)
    {
        qurl_slist_del_all(qurl_headers);
        qurl_headers = QOSA_NULL;
    }
    if (http_core != QOSA_NULL)
    {
        qurl_core_delete(http_core);
    }
    return err;
}