/**
 * @brief 开始录音并保存到文件。
 *
 * @param[in]  qosa_aud_rec_file_cfg_t *config
 *             - 音频录音配置参数结构体指针，包含录音文件名等信息。
 * @param[out] qosa_aud_handle_t *handle
 *             - 返回的音频句柄，用于后续控制和关闭录音。
 *
 * @return 成功返回 QOSA_AUD_SUCCESS，失败返回对应的错误码。
 */
#include "qosa_def.h"
#include "qosa_log.h"
#include "qcm_audio.h"
#include "qosa_queue_list.h"
#include "qosa_buffer_block.h"
#include "qosa_virtual_file.h"
#include "qcm_proj_config.h"
qosa_aud_errcode_e qcm_aud_record_file(qosa_aud_rec_file_cfg_t *config, qosa_aud_handle_t *handle)
{
    qosa_int32_t              ret = 0;
    qosa_aud_file_sys_t      *ctx = &gQAudFileMgr;
    qosa_aud_stream_cfg_t     stream_cfg = {0};
    qosa_aud_file_play_t     *file_ctx = QOSA_NULL;
    qosa_int32_t              write_size = 0;
    struct qosa_vfs_statvfs_t stat = {0};
    if (QOSA_NULL == config || QOSA_NULL == handle)
    {
        QLOGE("incalid param");
        return QOSA_AUD_INVALID_PARAM;
    }

    if((config->samprate != QOSA_AUD_RECORD_SAMPLE_RATE_8K) && (config->samprate != QOSA_AUD_RECORD_SAMPLE_RATE_16K))
    {
        config->samprate = QOSA_AUD_RECORD_SAMPLE_RATE_8K; //默认录音采样率8K
    }
    if (QOSA_NULL == ctx->record_lock)
    {
        qosa_mutex_create(&ctx->record_lock);
        qosa_q_init(&ctx->record_list);
    }

    if (qosa_mutex_lock(ctx->record_lock, QOSA_AUD_TIMEOUT) != QOSA_OK)
    {
        QLOGE("lock err");
        return QOSA_AUD_MUTEX_ERR;
    }

    if (qosa_q_cnt(&ctx->record_list))
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
    qosa_vfs_statvfs((const char *)config->file_name, &stat);
    g_file_free_size = stat.f_bsize * stat.f_bavail;
    file_ctx->fd = qosa_vfs_open((const char *)config->file_name, QOSA_VFS_O_WRONLY | QOSA_VFS_O_CREAT | QOSA_VFS_O_TRUNC);
    if (file_ctx->fd < 0)
    {
        QLOGE("file %s open err", config->file_name);
        qosa_aud_err_exit(QOSA_AUD_FILE_OPEN_ERR);
    }
    g_qosa_aud_file_record_stop = 0;
    g_write_hd.sample_rate = config->samprate;
    g_write_hd.byte_rate = (config->samprate * 1 * 16) / 8;
    write_size = qosa_vfs_write(file_ctx->fd, &g_write_hd, sizeof(wav_header_t));
    if (write_size != sizeof(wav_header_t))
    {
        QLOGE("file %s write err", config->file_name);
        qosa_aud_err_exit(QOSA_AUD_FILE_WRITE_ERR);
    }    stream_cfg.callback = qosa_file_event_hub;
    stream_cfg.format = QOSA_AUD_FMT_WAV;
    stream_cfg.samprate = config->samprate;
    stream_cfg.channels = 1;
    stream_cfg.sync_mode = QOSA_FALSE;
    stream_cfg.is_record = QOSA_TRUE;
    ctx->is_record = QOSA_TRUE;
    ctx->callback = config->callback;
    ret = qcm_aud_stream_open(&stream_cfg, &file_ctx->handle);
    if (QOSA_AUD_SUCCESS != ret)
    {
        QLOGE("stream open err");
        qosa_aud_err_exit(QOSA_AUD_SYSTEM_ERR);
    }

    qosa_q_link(QOSA_NULL, &file_ctx->link);
    qosa_q_put(&ctx->record_list, &file_ctx->link);

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
        }
    }
    else
    {
        QLOGD("file %s start record", config->file_name);
        *handle = file_ctx;
    }

    qosa_mutex_unlock(ctx->record_lock);
    return ret;
}