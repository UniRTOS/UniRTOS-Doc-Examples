/*****************************************************************/ /**
* @file http_fota_demo.c
* @brief HTTP FOTA Demo Implementation
* @author larson.li@quectel.com
* @date 2025-05-26
*
* @copyright Copyright (c) 2023 Quectel Wireless Solution, Co., Ltd.
* All Rights Reserved. Quectel Wireless Solution Proprietary and Confidential.
*
* @par EDIT HISTORY FOR MODULE
* <table>
* <tr><th>Date <th>Version <th>Author <th>Description
* <tr><td>2025-05-26 <td>1.0 <td>larson.li <td> Initial version
* </table>
**********************************************************************/
#include "qurl_code.h"
#include "qurl_api.h"
#include "qcm_file_api.h"
#include "qosa_fota.h"
#include "qosa_sys.h"
#include "qosa_def.h"
#include "qosa_log.h"
#include <string.h>
#include "qosa_datacall.h"
#include "qosa_power.h"

/*===========================================================================
 *  Macro Definition
 ===========================================================================*/
#define CONFIG_UNIRTOS_HTTP_FOTA_DEMO_TASK_STACK_SIZE 8192                                          // Demo task stack size configuration
#define UNIR_HTTP_FOTA_DEMO_TASK_PRIO                 QOSA_PRIORITY_NORMAL                          // Demo task priority configuration
#define UNIR_FOTA_PATH                                "http://112.31.84.164:8300/Bob/fotatest.par"  // FOTA package path configuration
#define UNIR_FOTA_PAKET_NAME                          "unir_fota_package.bin"                       // FOTA package filename configuration
#define UNIR_FOTAUPL_URL_MAX_LEN                      256                                           // Define maximum length for FOTA upload filename

/*===========================================================================
  *  struct
  ===========================================================================*/
/**
 * @struct unir_fota_http_t
 * @brief Defines unir_fota_http_t structure type, used to store various states and configuration information during HTTP transmission.
 */
typedef struct
{
    qurl_core_t   http_hd;                             /*!< HTTP header core information. */
    int           http_mode;                           /*!< HTTP mode. */
    int           ssl_ctxid;                           /*!< SSL context ID. */
    char          file_name[UNIR_FOTAUPL_URL_MAX_LEN]; /*!< File name to download or upload. */
    Q_FILE        fd;                                  /*!< File descriptor for file operations. */
    int           pdp_id;                              /*!< PDP context ID for mobile network data connection. */
    int           sim_id;                              /*!< SIM card ID. */
    qosa_uint32_t time_out;                            /*!< Timeout duration, unit may be milliseconds. */
    qosa_uint32_t start_pos;                           /*!< Starting position for download or upload. */
    qosa_uint32_t dload_want_size;                     /*!< Expected download size. */
    qosa_uint32_t resume_dload_count;                  /*!< Resume download count, used to handle restart after download interruption. */
    qosa_uint32_t fs_free_size;                        /*!< File system free space size. */
    int           event_errcode;                       /*!< Event error code, used to record errors that occur during the process. */
    int           write_errcode;                       /*!< Write error code, records errors in write operations. */
    int           chunk_encode;                        /*!< Chunk encoding flag, used to handle HTTP chunked transfer. */
    int           first_flag;                          /*!< Flag indicating whether it's the first transmission. */
    qosa_fota_t  *fota_ptr;                            /*!< FOTA operation handle */
    qosa_uint32_t write_size;                          /*!< Actual write size */
    qosa_uint32_t total_recv_cnt;                      /*!< Current accumulated total write size */
} unir_fota_http_t;

/*===========================================================================
  *  Enum
  ===========================================================================*/
/**
 * @brief HTTP FOTA error code enumeration definition
 */
typedef enum
{
    //FOTA success
    UNIR_FOAT_SUCCESS = QOSA_OK,
    UNIR_FOTA_HTTP_ERROR_UNKNOWN,
} unir_fota_error_e;

/*===========================================================================
 *  Macro Definition
 ===========================================================================*/
#define QOS_LOG_TAG LOG_TAG

/*===========================================================================
 *  Variate
 ===========================================================================*/
static qosa_task_t g_unir_http_fota_demo_task = QOSA_NULL;

/**
 * @brief HTTP response body data read callback function for handling data reception during FOTA download process.
 *
 * This function is called during HTTP data reception, responsible for parsing response header information
 * (only on first call), writing received data to FOTA module, and updating download status.
 *
 * @param buf  Pointer to the currently received data buffer
 * @param size Current received data length (in bytes)
 * @param arg  User-defined parameter, points to unir_fota_http_t structure
 *
 * @return Returns the actual processed data length. Returns 0 indicates error or download completion.
 */
static qosa_size_t http_read_body_cb(unsigned char *buf, long size, void *arg)
{
    qosa_fota_errno_e result = QOSA_FOTA_OK;
    long              resp_code = 0;              // Response status code
    long              content_length = -1;        // Response content length
    unir_fota_http_t *fota_http_ptr = QOSA_NULL;  // HTTP download parameter structure pointer
    QLOGD("size=%d", size);

    fota_http_ptr = (unir_fota_http_t *)arg;  // Get user-defined parameter
    // Initial processing on first call: get response status code and content length, determine download method
    if (fota_http_ptr->first_flag == 0)
    {
        // Get HTTP response status code and content length
        qurl_core_getinfo(fota_http_ptr->http_hd, QURL_INFO_RESP_CODE, &resp_code);
        qurl_core_getinfo(fota_http_ptr->http_hd, QURL_INFO_RESP_CONTENT_LENGTH, &content_length);

        // Set download method based on response status code and content length
        if (resp_code >= 200 && resp_code < 300)
        {
            if (content_length > 0)
            {
                // Check if file size exceeds available storage space
                if (content_length > fota_http_ptr->fs_free_size)
                {
                    QLOGE("download file size large");
                    fota_http_ptr->write_errcode = -1;
                    goto exit;
                }
                // Handle fixed content length download method
                fota_http_ptr->chunk_encode = 0;
                fota_http_ptr->dload_want_size = content_length;
            }
            else
            {
                // Handle chunked encoding download method
                fota_http_ptr->chunk_encode = 1;
                fota_http_ptr->dload_want_size = 0;
            }
            QLOGD("resp_code=%d,content_length=%d", resp_code, content_length);
        }
        else
        {
            // Abnormal response code, log error and exit
            QLOGE("resp_code=%d", resp_code);
            fota_http_ptr->event_errcode = -1;  // Error classification not actually used
            goto exit;
        }
        fota_http_ptr->first_flag = 1;  // Mark that initial state check is completed
    }

    // Write received data to FOTA module
    result = qosa_fota_write_packet_data(fota_http_ptr->fota_ptr, buf, size);
    if (result != QOSA_FOTA_OK)
    {
        // Write error, return 0 to terminate download
        return 0;
    }
    // Update total received data count and current write size
    fota_http_ptr->total_recv_cnt += size;
    fota_http_ptr->write_size += size;

    return size;  // Return the read data size

exit:
    return 0;
}

/**
 * @brief Start HTTP FOTA download process, configure and execute HTTP GET request to obtain firmware data.
 *
 * This function initializes HTTP handle, sets request parameters (such as URL, network ID, Range, etc.),
 * and executes HTTP request. Supports resumable download (via Range field) and redirect handling.
 *
 * @param fota_http_ptr Pointer to FOTA HTTP control structure, containing HTTP handle, PDP ID, timeout, etc.
 * @param url           Firmware download URL to request.
 *
 * @return Returns QOSA_OK on success; returns corresponding error code on failure.
 */
static int unirtos_fota_http_start(unir_fota_http_t *fota_http_ptr, char *url)
{
    qurl_core_t *http_hd_ptr = QOSA_NULL;
    qurl_ecode_t ret = QURL_OK;
    qurl_slist_t qurl_headers = QOSA_NULL;
    char         range_buf[32] = {0};

    // Global HTTP library initialization
    qurl_global_init();
    http_hd_ptr = &fota_http_ptr->http_hd;
    // If HTTP handle is null, create a new HTTP handle
    if (*http_hd_ptr == QOSA_NULL)
    {
        ret = qurl_core_create(http_hd_ptr);
        if (ret != QURL_OK)
        {
            fota_http_ptr->http_hd = QOSA_NULL;
            QLOGE("qurl_core_create failed");
            goto exit;
        }
    }

    // Print log information for debugging
    QLOGD("%p\r\n", *http_hd_ptr);
    QLOGD("url[%s]\r\n", url);
    QLOGD("pdp_id=%d", fota_http_ptr->pdp_id);

    // Set HTTP request parameters, including URL, network ID, request method, etc.
    qurl_core_setopt(*http_hd_ptr, QURL_OPT_URL, url);
    qurl_core_setopt(*http_hd_ptr, QURL_OPT_NETWORK_ID, fota_http_ptr->pdp_id);
    qurl_core_setopt(*http_hd_ptr, QURL_OPT_HTTP_GET, 1L);
    qurl_core_setopt(*http_hd_ptr, QURL_OPT_WRITE_CB, http_read_body_cb);
    qurl_core_setopt(*http_hd_ptr, QURL_OPT_WRITE_CB_ARG, fota_http_ptr);
    qurl_core_setopt(*http_hd_ptr, QURL_OPT_TIMEOUT_MS, 0L);
    qurl_core_setopt(*http_hd_ptr, QURL_OPT_IDLE_TIMEOUT_MS, fota_http_ptr->time_out * 1000);
    QLOGD("start_pos=%d", fota_http_ptr->start_pos);

    // If there is a specified starting position, set the Range field in HTTP request header for resumable download
    if (fota_http_ptr->start_pos > 0)
    {
        qosa_snprintf(range_buf, 32 - 1, "%d-", fota_http_ptr->start_pos);
        qurl_core_setopt(*http_hd_ptr, QURL_OPT_RANGE, range_buf);
    }

    // Enable HTTP redirect handling
    qurl_core_setopt(*http_hd_ptr, QURL_OPT_FOLLOWLOCATION, 1L);

    // Execute HTTP request
    ret = qurl_core_perform(*http_hd_ptr);
    QLOGD("ret=%x", ret);
    if (ret != QURL_OK)
    {
        QLOGE("%x\r\n", ret);
        QLOGE("qurl_core_perform failed");
        goto exit;
    }

    // Delete HTTP handle and reset related parameters after successful request
    qurl_core_delete(*http_hd_ptr);
    fota_http_ptr->http_hd = QOSA_NULL;

    return QOSA_OK;

exit:
    // Clean up resources: free request header list and HTTP handle
    if (qurl_headers != QOSA_NULL)
    {
        qurl_slist_del_all(qurl_headers);
        qurl_headers = QOSA_NULL;
    }
    if (fota_http_ptr->http_hd != QOSA_NULL)
    {
        qurl_core_delete(fota_http_ptr->http_hd);
        fota_http_ptr->http_hd = QOSA_NULL;
    }

    return ret;
}

/**
 * @brief Get available space size of FOTA partition
 *
 * This function is used to get the available space size of the partition where the specified file is located.
 * Current implementation gets the available space size of the root directory.
 *
 * @param fota_ptr   FOTA control block pointer
 * @param file_name  File name pointer
 *
 * @return Returns available partition space size in bytes.
 *         Returns negative value indicates failure to get the size.
 */
static qosa_int64_t unirtos_fota_get_partition_space(qosa_fota_t *fota_ptr, char *file_name)
{
    qosa_int64_t free_size;
    // Get remaining space size
    free_size = qcm_file_get_size(QCM_FS_GET_SIZE_FREE, "/");
    return free_size;
}

/**
 * @brief Activate FOTA network connection
 *
 * This function is used to activate network connection for specified SIM card and PDP context,
 * preparing network environment for FOTA upgrade.
 * If the specified PDP context is not activated, it attempts to activate it;
 * if already activated, it returns success directly.
 *
 * @param sim_id SIM card identifier, used to specify which SIM card to operate
 * @param pdp_id PDP context identifier, used to specify which PDP context to activate
 *
 * @return UNIR_FOAT_SUCCESS - Network activation successful
 * @return UNIR_FOTA_HTTP_ERROR_UNKNOWN - Network activation failed or parameter error
 */
unir_fota_error_e unir_fota_net_active(qosa_uint8_t sim_id, int pdp_id)
{
    qosa_datacall_errno_e   ret = QOSA_DATACALL_OK;
    qosa_datacall_ip_info_t info = {0};
    qosa_datacall_conn_t    conn = 0;

    // Record sim_id and pdp_id when called
    QLOGD("sim_id=%d,%d", sim_id, pdp_id);
    // Try to get information of specified PDP context
    conn = qosa_datacall_conn_new(sim_id, pdp_id, QOSA_DATACALL_CONN_TCPIP);
    ret = qosa_datacall_get_ip_info(conn, &info);
    if (ret == QOSA_DATACALL_ERR_NO_ACTIVE)
    {
        // If PDP context is not activated, try to activate it
        ret = qosa_datacall_start(conn, 30);
        // Record the result of activating PDP context
        QLOGV("fota active pdp ret: %x", ret);
        if (QOSA_DATACALL_OK != ret)
        {
            // If activation fails, log and return error code
            QLOGE("fota active pdp failed!");
            return UNIR_FOTA_HTTP_ERROR_UNKNOWN;
        }
        // Activation successful, return success status code
        return UNIR_FOAT_SUCCESS;
    }
    else if (ret == QOSA_DATACALL_OK)
    {
        // PDP context already activated, return success status code directly
        return UNIR_FOAT_SUCCESS;
    }

    // For other return values, indicates invalid parameters or other errors
    return UNIR_FOTA_HTTP_ERROR_UNKNOWN;
}

/**
 * @brief Release FOTA HTTP resources
 *
 * This function is used to release FOTA HTTP related resources, mainly cleaning up
 * HTTP handles after the upgrade process to avoid memory leaks.
 *
 * @param fota_http_ptr FOTA HTTP control block pointer, containing HTTP handle and other resource information
 *
 */
static void unirtos_fota_http_deinit(unir_fota_http_t *fota_http_ptr)
{
    /* If HTTP handle is valid, release the handle resource */
    if (fota_http_ptr->http_hd)
    {
        qurl_core_delete(fota_http_ptr->http_hd);
        fota_http_ptr->http_hd = QOSA_NULL;
    }
}

/**
 * @brief FOTA HTTP upgrade main processing function
 *
 * This function is used to execute FOTA firmware upgrade process based on HTTP protocol,
 * including network activation, FOTA initialization, file download, resumable download,
 * file verification, and upgrade flag setting operations.
 *
 * @param ctx Task context pointer (unused)
 *
 */
static void unir_http_fota_demo_process(void *ctx)
{
    int               ret = 0;
    unir_fota_http_t  fota_http_info = {0};
    qosa_int32_t      file_size = 0, free_size = 0;
    qosa_fota_errno_e fota_result = QOSA_FOTA_OK;

    // Delay 5s to prevent log loss
    qosa_task_sleep_sec(5);

    // Clear HTTP structure
    qosa_memset(&fota_http_info, 0, sizeof(unir_fota_http_t));

    // Disable the FOTA package signature verification
    qosa_set_fota_verify_config(QOSA_FOTA_VERIFY_DISABLE);

    // Query whether the signature verification for the FOTA package is enabled
    QLOGE("fota verify config %d", qosa_get_fota_verify_config());

    // Initialize HTTP download structure fields
    fota_http_info.sim_id = 0;
    fota_http_info.pdp_id = 1;
    fota_http_info.time_out = 60;
    fota_http_info.resume_dload_count = 0;
    // Set FOTA package name
    qosa_memcpy(fota_http_info.file_name, UNIR_FOTA_PAKET_NAME, qosa_strlen(UNIR_FOTA_PAKET_NAME) + 1);

    // Activate network
    ret = unir_fota_net_active(fota_http_info.sim_id, fota_http_info.pdp_id);
    if (ret != UNIR_FOAT_SUCCESS)
    {
        QLOGE("net failed");
        goto exit;
    }
    // Initialize FOTA and get FOTA handle
    fota_http_info.fota_ptr = qosa_fota_init(fota_http_info.file_name, QOSA_TRUE);
    if (fota_http_info.fota_ptr == QOSA_NULL)
    {
        QLOGE("qosa_fota_init failed");
        goto exit;
    }

    // Set upgrade URC count
    qosa_fota_set_update_urc_num(fota_http_info.fota_ptr, 10);
    // Maximum 3 download retries
    while (fota_http_info.resume_dload_count <= 3)
    {
        // Get current downloaded file size
        file_size = qosa_fota_get_current_file_size(fota_http_info.fota_ptr);
        if (file_size < 0)
        {
            ret = file_size;
            QLOGE("get size failed %d", ret);
            goto exit;
        }
        QLOGD("file_size=%d", file_size);
        // Set download start position and reset write size
        fota_http_info.start_pos = file_size;

        // Check if storage space is sufficient
        free_size = unirtos_fota_get_partition_space(fota_http_info.fota_ptr, fota_http_info.file_name);
        QLOGD("start_pos=%d,free_size=%ld", fota_http_info.start_pos, free_size);
        if (free_size < 0)
        {
            ret = free_size;
            QLOGE("get size failed %d", ret);
            goto exit;
        }
        // Set remaining space
        fota_http_info.fs_free_size = (qosa_uint32_t)free_size;

        // Start download, pass corresponding URL
        ret = unirtos_fota_http_start(&fota_http_info, UNIR_FOTA_PATH);
        QLOGD("cnt=%d,%d,%d", fota_http_info.total_recv_cnt, fota_http_info.write_size, fota_http_info.event_errcode);

        if (ret != UNIR_FOAT_SUCCESS)
        {
            QLOGE("http failed %d", ret);
            goto exit;
        }
        // Check if file size meets expectations
        file_size = qosa_fota_get_current_file_size(fota_http_info.fota_ptr);
        if (file_size < 0)
        {
            QLOGE("get size failed %d", file_size);
            goto exit;
        }
        // If download is not completed, perform resumable download logic
        if (fota_http_info.dload_want_size > file_size)
        {
            QLOGV("breakpoint resume count :%d", fota_http_info.resume_dload_count++);
            qosa_task_sleep_sec(6);
            unirtos_fota_http_deinit(&fota_http_info);
        }
        else
        {
            // Download completed, exit loop
            break;
        }
    }
exit:
    // Execute corresponding operations based on download result
    if (ret == UNIR_FOAT_SUCCESS)
    {
        // Verify downloaded firmware image
        fota_result = qosa_fota_verify_image(fota_http_info.fota_ptr);
        if (fota_result != QOSA_FOTA_OK)
        {
            QLOGE("verify failed fota_result:%d", fota_result);
        }
        else
        {
            // Verification successful, set flag and reboot to enter upgrade
            qosa_fota_flag_set();
            qosa_power_reset(QOSA_RESET_FOTA);  // Reboot to enter upgrade
        }
    }

    // Resource cleanup before exit
    QLOGV("exit");
    qosa_fota_deinit(fota_http_info.fota_ptr);
    unirtos_fota_http_deinit(&fota_http_info);
    return;
}

void unir_http_fota_demo_init(void)
{
    QLOGV("enter UniRTOS http fota DEMO !!!");
    if (g_unir_http_fota_demo_task == QOSA_NULL)
    {
        qosa_task_create(
            &g_unir_http_fota_demo_task,
            CONFIG_UNIRTOS_HTTP_FOTA_DEMO_TASK_STACK_SIZE,
            UNIR_HTTP_FOTA_DEMO_TASK_PRIO,
            "http_fota_demo",
            unir_http_fota_demo_process,
            QOSA_NULL,
            1
        );
    }
}
