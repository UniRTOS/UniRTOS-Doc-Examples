/**
 * @brief 播放指定的音频文件
 *
 * @param[in]  qosa_aud_file_cfg_t *config
 *              - 音频文件配置结构体指针，包含要播放的文件名等信息
 * @param[out] qosa_aud_handle_t *handle
 *              - 返回创建的音频播放句柄，用于后续控制播放操作
 *
 * @return qosa_aud_errcode_e
 *         - QOSA_AUD_SUCCESS: 成功启动播放
 *         - 其他 播放失败
  */
#include "qosa_def.h"
#include "qosa_log.h"
#include "qcm_audio.h"
#include "qosa_queue_list.h"
#include "qosa_buffer_block.h"
#include "qosa_virtual_file.h"
#include "qcm_proj_config.h"
qosa_aud_errcode_e qcm_aud_play_file(qosa_aud_file_cfg_t *config, qosa_aud_handle_t *handle)
{
    qosa_int32_t          ret = 0;
    qosa_aud_file_sys_t  *ctx = &gQAudFileMgr;
    qosa_aud_stream_cfg_t stream_cfg = {0};
    qosa_aud_file_play_t *file_ctx = QOSA_NULL;
    qosa_aud_head_info_t  info = {0};
    qosa_aud_fmt_e        fmt = QOSA_AUD_FMT_UNKNOWN;

    if (QOSA_NULL == config || QOSA_NULL == handle)
    {
        QLOGE("incalid param");
        return QOSA_AUD_INVALID_PARAM;
    }

    if (QOSA_NULL == ctx->play_lock)
    {
        qosa_mutex_create(&ctx->play_lock);
        qosa_q_init(&ctx->play_list);
    }

    if (qosa_mutex_lock(ctx->play_lock, QOSA_AUD_TIMEOUT) != QOSA_OK)
    {
        QLOGE("lock err");
        return QOSA_AUD_MUTEX_ERR;
    }

    if (qosa_q_cnt(&ctx->play_list))
    {
        QLOGE("player reopen");
        qosa_aud_err_exit(QSOA_AUD_REOPEN_ERR);
    }
    file_ctx = qosa_calloc(1, sizeof(qosa_aud_file_play_t));
    if (QOSA_NULL == file_ctx)
    {
        QLOGE("no memory");
        qosa_aud_err_exit(QOSA_AUD_NO_MEMORY_ERR);
    }
    qosa_memset(file_ctx, 0, sizeof(qosa_aud_file_play_t));
    file_ctx->fd = qosa_vfs_open((const char *)config->file_name, QOSA_VFS_O_RDONLY);
    if (file_ctx->fd < 0)
    {
        QLOGE("file %s open err", config->file_name);
        qosa_aud_err_exit(QOSA_AUD_FILE_OPEN_ERR);
    }    g_qosa_aud_file_stop = 0;
    fmt = qosa_aud_fmt_check(&info, config->file_name, file_ctx->fd);
    if (QOSA_AUD_FMT_UNKNOWN == fmt)
    {
        QLOGE("file %s invalid", config->file_name);
        qosa_aud_err_exit(QOSA_AUD_INVALID_PARAM);
    }
    QLOGD("head_size %d fmt %d", info.head_size, fmt);
    if (info.head_size > 0)
    {
        qosa_vfs_lseek(file_ctx->fd, info.head_size, QOSA_VFS_SEEK_SET);  //文件指针跳转到数据帧起始位置
    }
    file_ctx->head_size = info.head_size;
    if(config->options & 0x01)
    {
        file_ctx->is_repeat = QOSA_TRUE;
    }    stream_cfg.callback = qosa_file_event_hub;
    stream_cfg.format = fmt;
    stream_cfg.samprate = info.samprate;  //此处不一定解析到了采样率信息,如果没有解析到,会在后续数据帧解析流程中采集
    stream_cfg.channels = info.channles;  //此处不一定解析到了通道数信息,如果没有解析到,会在后续数据帧解析流程中采集
    stream_cfg.sync_mode = QOSA_FALSE;
    ctx->is_record = QOSA_FALSE;
    ctx->callback = config->callback;
    ret = qcm_aud_stream_open(&stream_cfg, &file_ctx->handle);
    if (QOSA_AUD_SUCCESS != ret)
    {
        QLOGE("stream open err");
        qosa_aud_err_exit(QOSA_AUD_SYSTEM_ERR);
    }

    qosa_q_link(QOSA_NULL, &file_ctx->link);
    qosa_q_put(&ctx->play_list, &file_ctx->link);
exit:

    if (QOSA_AUD_SUCCESS != ret)
    {
        if (0 != file_ctx->fd)
        {
            qosa_vfs_close(file_ctx->fd);
        }

        if (QOSA_NULL != file_ctx)
        {
            qosa_free(file_ctx);
            file_ctx = QOSA_NULL;
        }
    }
    else
    {
        QLOGD("file %s start play", config->file_name);
        *handle = file_ctx;
    }
    qosa_mutex_unlock(ctx->play_lock);
    return ret;
}