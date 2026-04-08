#include "quecos_atcmd_cfg.h"
#include "quecos_http_app.h"
#include "qosa_def.h"
#include "qosa_sys.h"
#include "qosa_log.h"
#include "qurl.h"
#include "qurl_tls.h"

//自定义请求头数据
static char  head_data[] = "GET / HTTP/1.1\r\nHost: www.baidu.com\r\nAccept: */*\r\n\r\n";
static char g_buff[2049] = {0};

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
 * @brief 通过qurl实现https get功能演示
 *
 * 该函数实现了一个 https get 操作的完整流程，展示了如何基于qurl系统发起 https get 请求去获取http服务器上的资源。
 * 主要实现了 qurl 相关配置项的配置以及 qurl 请求的发起。
 * 包括一个新的 qurl 实例的创建用于发起 https get请求。
 * 配置 https get 请求的url。
 * 配置pdp通道，qurl传输操作总的超时时间以及http请求方法。
 * 配置用户读取请求 header 的指针以及请求header的长度。读取请求 header 的一种方式：通过指针去读取请求header。
 * 配置 https 请求使用自定义请求头。
 * 配置用户接收回应 body 的回调函数以及传入回调函数中的用户参数。
 * 配置用户接收回应 header 的回调函数以及传入回调函数中的用户参数。
 * 初始化tls配置并配置tls相关参数。
 * qurl_core_perform发起 https get请求。等待请求结果并通过回调函数处理 https get 过程中的数据。
 * 请求结束删除之前创建的qurl实例。
 */
qosa_int32_t https_get_demo(qurl_core_t http_core)
{
    qosa_int32_t    err = QOSA_OK;
    qurl_ecode_t    ret = QURL_OK;
    qurl_tls_cfg_t  tls_cfg = {0};
    int g_http_last_close_event = 0;

    qurl_global_init();

    if (http_core == QOSA_NULL)
    {
        //没有可用的 qurl 实例，创建一个新的 qurl 实例用于发起 https get 请求。
        ret = qurl_core_create(&http_core);
        if (ret != QURL_OK)
        {
            err = OSA_HTTP_ERR_FAILURE;
            goto exit;
        }
    }

    QLOGV("%p\r\n", http_core);
    //配置 https get请求的url
    qurl_core_setopt(http_core, QURL_OPT_URL, "https://www.baidu.com");
    //配置使用的pdp通道为1
    qurl_core_setopt(http_core, QURL_OPT_NETWORK_ID, 1);
    //配置qurl传输操作总的超时时间为30s
    qurl_core_setopt(http_core, QURL_OPT_TIMEOUT_MS, 30 * 1000);
    //配置http请求方法为 GET 请求
    qurl_core_setopt(http_core, QURL_OPT_HTTP_GET, 1L);
    //配置用户读取请求 header 的指针，读取请求 header 的一种方式，其他方法可以参考通用配置项中的QURL_OPT_READ_HEAD_CB和QURL_OPT_UPLOAD_HEAD_FILE
    qurl_core_setopt(core, QURL_OPT_UPLOAD_HEAD_DATA, head_data);
    qurl_core_setopt(core, QURL_OPT_UPLOAD_HEAD_SIZE, qosa_strlen(head_data));
    //http 请求使用自定义请求头,这里通过指针读取自定义请求头
    qurl_core_setopt(http_core, QURL_OPT_UPLOAD_HEAD_RAW, 1L);
    //配置用户接收回应 body 的回调函数
    qurl_core_setopt(http_core, QURL_OPT_WRITE_CB, write_to_user_func);
    //配置传入接收回应 body 回调函数中的用户参数，用户可根据需要传入想要的参数
    //qurl_core_setopt(http_core, QURL_OPT_WRITE_CB_ARG, write_to_user_arg);
    //配置用户接收回应 header 的回调函数
    qurl_core_setopt(http_core, QURL_OPT_WRITE_HEAD_CB, http_write_head_cb);
    //配置传入接收回应 header 回调函数中的用户参数，用户可根据需要传入想要的参数
    //qurl_core_setopt(http_core, QURL_OPT_WRITE_HEAD_CB_ARG, http_write_head_cb_arg);
    //初始化tls配置并配置tls相关参数
    qurl_tls_cfg_init(&tls_cfg);
    tls_cfg.negotiate_timeout = 30;
    qurl_core_setopt(core, QURL_OPT_TLS_CFG, &tls_cfg);
    //发起 https get 请求，阻塞等待请求的结果
    ret = qurl_core_perform(http_core);
    //获取 qurl 实例最后关闭的原因
    g_http_last_close_event = qurl_core_get_last_close_event(http_core);

    if (ret != QURL_OK)
    {
        QLOGE("%x\r\n", ret);
        err = OSA_HTTP_ERR_FAILURE;
        goto exit;
    }
    //请求结束删除之前创建的qurl实例
    qurl_core_delete(http_core);

    return QOSA_OK;
exit:
    if (http_core != QOSA_NULL)
    {
        qurl_core_delete(http_core);
    }
    return err;
}