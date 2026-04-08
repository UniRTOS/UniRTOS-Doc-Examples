#include "qosa_sys.h"
#include "qosa_def.h"
#include "qosa_log.h"
#include "qurl.h"
#include "qosa_virtual_file.h"

static char g_form_data2[] = "this is test2";
/**
*@brief HTTP应用层读取表单数据回调函数
*
*@param buf 用于存储读取数据的缓冲区指针
*@param size 请求的读取数据大小
*@param arg 指向用户传递数据的指针
*
*@return实际上传的数据大小，返回0表示没有要读取的数据
*/
long qurl_http_app_r_form_cb(unsigned char *buf, long size, void *arg)
{
    qurl_app_r_buf_t *r_buf_ptr = (qurl_app_r_buf_t *)arg;

    /* Check if requested write data size is valid */
    if (size < 1)
    {
        return 0;
    }

    /* Calculate actual writable data size to avoid exceeding buffer boundaries */
    size = QURL_HTTP_APP_MIN(size, r_buf_ptr->buf_len - r_buf_ptr->r_index);

    /* Copy data from buffer to target buffer */
    qosa_memcpy(buf, r_buf_ptr->buf_ptr + r_buf_ptr->r_index, size);

    /* Update write index position */
    r_buf_ptr->r_index += size;

    /* All written */
    if (r_buf_ptr->r_index >= r_buf_ptr->buf_len)
    {
        r_buf_ptr->r_index = 0;
    }

    return size;
}
//用户接收回应 header 的回调函数，示例只是将回应 header 打印出来，实际使用可以根据需要进行特定的处理。
static long qurl_http_app_w_h_cb(char *buf, long size, void *arg)
{
    // Prevent unused parameter warning
    QOSA_UNUSED(arg);

    // Record received data size and content to application log
    QLOGD("size=%d,%s", size, buf);
    return size;
}
//用户接收回应 body 的回调函数，示例只是将回应 body 打印出来，实际使用可以根据需要进行特定的处理。
static long qurl_http_app_w_cb(char *buf, long size, void *arg)
{
    QOSA_UNUSED(arg);

    QLOGD("size=%d,%s", size, buf);
    return size;
}

/**
 * @brief 通过 qurl 实现 http post多媒体表单功能演示
 *
 * 该函数实现了一个 http post多媒体表单操作的完整流程，展示了如何基于qurl系统发起 http post 请求去向服务器提交多媒体表单。
 * 主要实现了 qurl 相关配置项的配置以及 qurl 请求的发起。
 * 初始化qurl全局资源。
 * 创建一个新的 qurl 实例的创建用于发起 http post 多媒体表单请求。
 * 配置 http post 多媒体表单请求的url。
 * 配置pdp通道以及 http 请求方法。
 * 添加第一个表单字段：直接上传字符串数据。
 * 添加第二个表单字段：从全局变量上传字符串数据。
 * 添加第三个表单字段：通过回调函数上传数据。
 * 配置用户接收回应 header 的回调函数。
 * 配置用户接收回应 body 的回调函数。
 * qurl_core_perform发起 http post 多媒体表单请求。等待请求结果并通过回调函数处理 http post 多媒体表单过程中的数据。
 * 请求结束获取 http 服务器返回的响应状态码以及响应内容长度。
 * 请求结束删除之前创建的 qurl 实例。
 */
qurl_ecode_t http_post_form_demo(void)
{
    qurl_ecode_t         ret = QURL_OK;
    qurl_core_t          core = QOSA_NULL;
    long                 resp_code = 0;
    long                 content_length = 0;
    qurl_http_form_cfg_t form_cfg = {0};
    qurl_app_r_buf_t     r_buff = {0};

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

    // 初始化全局资源，开机调用一次
    qurl_global_init();


    // 创建一个新的 qurl 实例用于发起 http post 多个多媒体表单请求。
    ret = qurl_core_create(&core);
    if (ret != QURL_OK)
    {
        QLOGE("%x\r\n", ret);
    }

    //配置 http post多表单请求的url
    ret = qurl_core_setopt(core, QURL_OPT_URL, "https://www.baidu.com");
    if (ret != QURL_OK)
    {
        QLOGE("%x\r\n", ret);
    }

    //配置使用的pdp通道为1
    qurl_core_setopt(core, QURL_OPT_NETWORK_ID, 1);
    //配置http请求方法为 POST 多媒体表单请求
    qurl_core_setopt(core, QURL_OPT_HTTP_POST_FORM, 1L);

    //添加第一个表单字段：直接上传字符串数据
    //整个表单的名字
    form_cfg.name_ptr = "test1";
    //多媒体表单是一个文件，表示文件的名字
    form_cfg.filename_ptr = "file1";
    //获取多媒体表单数据的方式
    form_cfg.content_type = QURL_HTTP_FORM_CONTENT_DATA;
    //直接上传字符串数据的方式去获取多媒体表单数据
    form_cfg.content_ptr = "this is test1";
    //通过回调函数去获取表单数据的方式需要使用到
    form_cfg.read_content_func = QOSA_NULL;
    //要发送的内容字段的长度
    form_cfg.content_len = qosa_strlen(form_cfg.content_ptr);
    //添加要上传的多媒体表单
    qurl_core_setopt(core, QURL_OPT_FORM, 1L, &form_cfg);

    //添加第二个表单字段：从全局变量上传字符串数据
    form_cfg.name_ptr = "test2";
    form_cfg.filename_ptr = "file2";
    form_cfg.content_type = QURL_HTTP_FORM_CONTENT_DATA;
    form_cfg.content_ptr = g_form_data2;
    form_cfg.read_content_func = QOSA_NULL;
    form_cfg.content_len = qosa_strlen(g_form_data2);
    qurl_core_setopt(core, QURL_OPT_FORM, 2L, &form_cfg);

    //添加第三个表单字段：通过回调函数上传数据
    r_buff.buf_ptr = qosa_malloc(20);
    qosa_memset(r_buff.buf_ptr, 0, 20);
    qosa_snprintf(r_buff.buf_ptr, 19, "%s", "this is test3");
    r_buff.r_index = 0;
    r_buff.buf_len = qosa_strlen(r_buff.buf_ptr);
    form_cfg.name_ptr = "test3";
    form_cfg.filename_ptr = "file3";
    form_cfg.content_type = QURL_HTTP_FORM_CONTENT_CB;
    form_cfg.read_content_func = qurl_http_app_r_form_cb;
    form_cfg.content_ptr = &r_buff;
    form_cfg.content_len = r_buff.buf_len;
    qurl_core_setopt(core, QURL_OPT_FORM, 3L, &form_cfg);

    //设置响应头和响应体回调处理函数
    qurl_core_setopt(core, QURL_OPT_WRITE_HEAD_CB, qurl_http_app_w_h_cb);
    qurl_core_setopt(core, QURL_OPT_WRITE_CB, qurl_http_app_w_cb);

    //发起 http post 多媒体表单请求，阻塞等待请求的结果
    ret = qurl_core_perform(core);
    if (ret != QURL_OK)
    {
        QLOGE("%x\r\n", ret);
    }

    //获取http服务器返回的响应状态码
    qurl_core_getinfo(core, QURL_INFO_RESP_CODE, &resp_code);
    QLOGV("resp_code:[%d]\r\n", resp_code);

    //获取响应内容长度
    qurl_core_getinfo(core, QURL_INFO_RESP_CONTENT_LENGTH, &content_length);
    QLOGV("content_length:[%d]\r\n", content_length);

    //请求结束删除之前创建的qurl实例，释放资源
    ret = qurl_core_delete(core);
    if (ret != QURL_OK)
    {
        QLOGE("%x\r\n", ret);
    }
    qosa_free(r_buff.buf_ptr);

    return QURL_OK;
}