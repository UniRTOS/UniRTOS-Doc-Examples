/*****************************************************************/ /**
* @file ftp_fota_demo.c
* @brief FTP FOTA Demo Implementation
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
#include "qcm_file_api.h"
#include "qurl_code.h"
#include "qurl_def.h"
#include "qosa_sys.h"
#include "qosa_def.h"
#include "qosa_log.h"
#include "qosa_fota.h"
#include <string.h>
#include "ftp_fota_demo.h"
#include "qurl_api.h"
#include "qosa_datacall.h"
#include "qosa_power.h"

/*===========================================================================
 *  Macro Definition
 ===========================================================================*/
#define CONFIG_UNIRTOS_FTP_FOTA_DEMO_TASK_STACK_SIZE 8192                                                      // Demo task stack size configuration
#define UNIR_FTP_FOTA_DEMO_TASK_PRIO                 QOSA_PRIORITY_NORMAL                                      // Demo task priority configuration
#define UNIR_FOTA_PATH_NAME                          "ftp://test:test@112.31.84.164:8309/BOB/default_ftp.par"  // FOTA upgrade package path name configuration
#define UNIR_FOTA_URL                                "ftp://112.31.84.164:8309/BOB/default_ftp.par"            // FOTA upgrade URL configuration
#define UNIR_FOTA_PAKET_NAME                         "unir_fota_package.bin"                                   // FOTA upgrade package filename configuration
#define UNIR_FOTAUPL_URL_MAX_LEN                     256                                                       // FOTA upgrade URL maximum length configuration

/*===========================================================================
 * callback
===========================================================================*/
typedef qosa_size_t (*UNIR_FTP_FOTA_WRITE_CB)(void *ptr, qosa_size_t size, void *stream);

/*===========================================================================
  *  struct
  ===========================================================================*/
/**
 * @struct fota_ftp_info_t
 * @brief Defines the fota_ftp_info_t structure type, used to store FTP transmission related information during FOTA upgrade process.
 */
typedef struct
{
    int           sim_id;                              /*!< SIM card identifier */
    int           pdp_id;                              /*!< PDP context identifier */
    char          path_name[UNIR_FOTAUPL_URL_MAX_LEN]; /*!< File path name */
    char          file_name[QCM_FILE_MAX_PATH_LEN];    /*!< Locally stored file name */
    qosa_uint32_t current_dl_offset;                   /*!< Currently downloaded length in bytes */
    double        current_dl_len;                      /*!< Current length to download in bytes */
    int           error_code;                          /*!< Error code, used to indicate error status during transmission */
    qosa_fota_t  *fota_ptr;                            /*!< FOTA operation handle */
} fota_ftp_info_t;

/**
 * @struct unir_ftp_context_t
 * @brief Defines the unir_ftp_context_t structure type, used to store FTP connection and operation related configuration and status information.
 */
typedef struct
{
    qurl_core_t    qurl;                               /*!< URL core configuration information */
    qurl_tls_cfg_t qurl_tls_cfg;                       /*!< TLS security configuration information */
    char           hostname[UNIR_FOTAUPL_URL_MAX_LEN]; /*!< FTP server hostname */
    char           username[UNIR_FOTAUPL_URL_MAX_LEN]; /*!< Username */
    char           password[UNIR_FOTAUPL_URL_MAX_LEN]; /*!< Password */
    int            timeout;                            /*!< Connection timeout time (seconds) */
    int            server_port;                        /*!< FTP server port number */
    int            ssl_enable;                         /*!< Encryption method */
    int            transfer;                           /*!< Data transfer mode */
    int            skip_pasv_ip;                       /*!< Whether to skip passive mode IP check */
    int            status;                             /*!< Current connection status */
    int            sslCtx;                             /*!< SSL context */
    int            pdp_cid;                            /*!< PDP context identifier */
    int            debug_enable;                       /*!< Debug function enable flag */
    int            ftp_port;                           /*!< FTP port mode, default uses passive mode UNIR_FTP_PASV_MODE */
    int            ipv6_extend;                        /*!< IPv6 extension function enable */
    int            last_reply_code;                    /*!< Last server response code */

    char *currentDir;                                  /*!< Current working directory */
    char *last_reply_Dir;                              /*!< Directory information in last response */
    char *server_ap;                                   /*!< FTP server absolute path */
} unir_ftp_context_t, *unir_ftp_context_ptr;

/**
 * @struct unir_ftp_fota_url_parts_t
 * @brief URL parsing result structure
 */
typedef struct
{
    char *username;   /*!< Username */
    char *password;   /*!< Password */
    char *server_url; /*!< Server URL */
    int   port;       /*!< Port number */
    char *file_path;  /*!< File path */
} unir_ftp_fota_url_parts_t;

/*===========================================================================
  *  Enum
  ===========================================================================*/
/**
 * @brief FTP FOTA error code enumeration definition
 */
typedef enum
{
    // FOTA success
    UNIR_FOAT_SUCCESS = QOSA_OK,
    UNIR_FOTA_FTP_ERROR_UNKNOWN,
} unir_fota_error_e;


/*===========================================================================
 *  Macro Definition
 ===========================================================================*/
#define QOS_LOG_TAG LOG_TAG

/*===========================================================================
 *  Variate
 ===========================================================================*/
static qosa_task_t g_unir_ftp_fota_demo_task = QOSA_NULL;

/**
 * @brief Activate network PDP context for FOTA functionality
 *
 * This function activates the network connection for the specified SIM card and PDP context ID.
 * If the PDP context is not already activated, it attempts to activate it;
 * if already activated, it returns success status directly.
 *
 * @param[in] sim_id  SIM card identifier to specify which SIM to operate on
 * @param[in] pdp_id  PDP context ID to specify which PDP context to activate
 *
 * @return Operation result status code
 *         - UNIR_FOAT_SUCCESS: Network activation successful or already active
 *         - UNIR_FOTA_FTP_ERROR_UNKNOWN: Invalid parameters, activation failure, or other errors
 */
unir_fota_error_e unir_fota_net_active(qosa_uint8_t sim_id, int pdp_id)
{
    qosa_datacall_errno_e   ret = QOSA_DATACALL_OK;
    qosa_datacall_ip_info_t info = {0};
    qosa_datacall_conn_t    conn = 0;

    // Print sim_id and pdp_id information for debugging
    QLOGD("sim_id=%d,%d", sim_id, pdp_id);
    // Attempt to get information for the specified PDP context
    conn = qosa_datacall_conn_new(sim_id, pdp_id, QOSA_DATACALL_CONN_TCPIP);
    ret = qosa_datacall_get_ip_info(conn, &info);
    if (ret == QOSA_DATACALL_ERR_NO_ACTIVE)
    {
        // If PDP context is not activated, attempt to activate it
        ret = qosa_datacall_start(conn, 30);
        // Record the result of PDP context activation
        QLOGE("fota active pdp ret: %x", ret);
        if (QOSA_DATACALL_OK != ret)
        {
            // If activation fails, log and return error code
            QLOGE("fota active pdp failed!");
            return UNIR_FOTA_FTP_ERROR_UNKNOWN;
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
    return UNIR_FOTA_FTP_ERROR_UNKNOWN;
}

/**
 * @brief Create and initialize a new FTP client configuration context
 *
 * This function allocates a new FTP context structure and initializes it,
 * including setting default FTP connection parameters. If called for the first time,
 * it also initializes the global FTP library.
 *
 * @return Returns pointer to the newly created FTP context on success; returns QOSA_NULL on failure.
 */
unir_ftp_context_ptr unir_ftp_client_new_cfg(void)
{
    // Allocate memory for FTP context structure
    unir_ftp_context_ptr ctx_ptr = (unir_ftp_context_ptr)qosa_malloc(sizeof(unir_ftp_context_t));
    static int           ftp_init_flag = 0;

    // Check if memory allocation was successful
    if (ctx_ptr == QOSA_NULL)
    {
        QLOGE("mem err");
        return QOSA_NULL;
    }

    // Initialize global FTP library on first call
    if (ftp_init_flag == 0)
    {
        qurl_global_init();
    }

    // Initialize allocated memory space
    qosa_memset(ctx_ptr, 0x00, sizeof(unir_ftp_context_t));

    // Set FTP client default configuration parameters
    ctx_ptr->pdp_cid = 1;                 // PDP context identifier
    ctx_ptr->timeout = 90;                // Timeout (seconds)
    ctx_ptr->ssl_enable = 0;              // SSL encryption mode: 0-FTP, 1-implicit encryption, 2-explicit encryption, 3-SFTP
    ctx_ptr->sslCtx = 0;                  // SSL context
    ctx_ptr->status = 0;                  // Connection status
    ctx_ptr->debug_enable = 1;            // Debug information switch
    ctx_ptr->last_reply_code = 0;         // Last response code
    ctx_ptr->ftp_port = 1;                // FTP port mode
    ctx_ptr->skip_pasv_ip = 2;            // Passive mode IP skip setting
    ctx_ptr->currentDir = QOSA_NULL;      // Current directory path
    ctx_ptr->last_reply_Dir = QOSA_NULL;  // Last response directory
    ctx_ptr->server_ap = QOSA_NULL;       // Server access point
    ctx_ptr->ipv6_extend = 1;             // Enable IPv6 extension

    return ctx_ptr;
}

/**
 * @brief Clean up FTP client context resources
 *
 * This function releases the memory resources pointed to by the FTP client context pointer,
 * and logs cleanup information.
 *
 * @param ctx_ptr FTP client context pointer
 * @return No return value
 */
void unir_ftp_client_clean(unir_ftp_context_ptr ctx_ptr)
{
    // Release memory resources pointed to by context pointer
    if (ctx_ptr != QOSA_NULL)
    {
        qosa_free(ctx_ptr);
        ctx_ptr = QOSA_NULL;
    }
    QLOGV("clean");
}

/**
 * @brief Create and initialize a new FTP client context
 *
 * This function creates a new FTP client context and performs basic configuration,
 * including setting default port, username, password and other information.
 *
 * @param fota_ftp_info Pointer to FOTA FTP information structure, containing FTP server related information
 *
 * @return Returns pointer to the newly created FTP client context on success, returns QOSA_NULL on failure
 */
static unir_ftp_context_ptr unir_fota_ftp_handle_new(fota_ftp_info_t *fota_ftp_info)
{
    unir_ftp_context_ptr ctx_ptr = QOSA_NULL;

    // First check if context pointer is null to ensure safe subsequent operations
    if (ctx_ptr == QOSA_NULL)
    {
        // Create new FTP client context
        ctx_ptr = unir_ftp_client_new_cfg();
        if (ctx_ptr == QOSA_NULL)
        {
            return QOSA_NULL;  // Creation failed, return directly
        }
        // Configure FTP client basic connection parameters
        ctx_ptr->server_port = 8309;
        qosa_memcpy(ctx_ptr->username, "test", qosa_strlen("test") + 1);
        qosa_memcpy(ctx_ptr->password, "test", qosa_strlen("test") + 1);
        QLOGV("Username: %s\n", ctx_ptr->username);
        QLOGV("password: %s\n", ctx_ptr->password);

        // Record context pointer for subsequent operations and debugging
        QLOGV("ftp_ctx :%p", ctx_ptr);
    }

    return ctx_ptr;  // Return configured FTP client context pointer
}

/**
 * @brief Get available space size for FOTA upgrade partition
 *
 * @param fota_ptr FOTA control structure pointer
 * @param file_name File name parameter (unused)
 * @return qosa_int64_t Returns available space size
 */
static qosa_int64_t unirtos_fota_get_partition_space(qosa_fota_t *fota_ptr, char *file_name)
{
    QOSA_UNUSED(fota_ptr);
    qosa_int64_t free_size;
    // Get remaining space size
    free_size = qcm_file_get_size(QCM_FS_GET_SIZE_FREE, "/");
    return free_size;
}

/**
 * @brief FTP data transmission callback function, used for processing data reception and storage during FOTA upgrade process
 *
 * @param ptr Pointer to received data buffer
 * @param size Current received data block size
 * @param stream Pointer to FTP information structure, containing download status and FOTA related information
 *
 * @return Returns actual processed data size, returns 0 if processing fails
 */
static qosa_size_t unir_fota_ftp_transmitter_cb(void *ptr, qosa_size_t size, void *stream)
{
    int              realsize = 0;
    int              inbuflen = size;
    fota_ftp_info_t *info_msg = (fota_ftp_info_t *)stream;
    QLOGD("size=%d", inbuflen);

    // Check if input parameters are valid
    if (ptr == QOSA_NULL)
    {
        info_msg->error_code = -1;
        return 0;
    }

    // Calculate actual transmission size based on current download length and offset
    if (info_msg->current_dl_len != 0xFFFFFFFF)
    {
        realsize = MIN(inbuflen, (info_msg->current_dl_len - info_msg->current_dl_offset));
    }
    else
    {
        realsize = inbuflen;
    }

    // If calculated actual transmission size is 0, indicates transmission completed
    if (realsize == 0)
    {
        QLOGV("get over!!");
        info_msg->error_code = -2;
        return 0;
    }

    // Write data packet to fota storage area
    qosa_fota_errno_e result = QOSA_FOTA_OK;
    result = qosa_fota_write_packet_data(info_msg->fota_ptr, ptr, size);
    if (result != QOSA_FOTA_OK)
    {
        return 0;
    }

    // Update current download offset and set error code to 0, indicating success
    info_msg->current_dl_offset += size;
    info_msg->error_code = 0;
    QLOGD("current_dl_offset=%d", info_msg->current_dl_offset);

    return size;
}

/**
 * @brief FTP request reply processing function
 *
 * This function processes FTP request reply data and calculates actual received data size
 *
 * @param ptr Pointer to received data
 * @param size Size of each data item (bytes)
 * @param stream Pointer to data stream
 *
 * @return Returns actual received data size (bytes)
 */
static qosa_size_t unir_ftp_request_reply(void *ptr, qosa_size_t size, void *stream)
{
    qosa_uint32_t realsize = size;
    return realsize;
}

/**
 * @brief Get FTP reply code
 *
 * This function processes FTP server reply information and extracts reply code
 *
 * @param ptr Pointer to received data buffer
 * @param size Received data size
 * @param stream User-defined data stream pointer
 *
 * @return Returns actual processed data size
 */
static qosa_size_t unir_get_ftp_repaly_code(void *ptr, qosa_size_t size, void *stream)
{
    qosa_uint32_t realsize = size;
    return realsize;
}

/**
 * @brief Get file size from specified URL using FTP protocol (for FOTA upgrade scenarios)
 *
 * This function uses FTP protocol to connect to specified server, configures username,
 * password, port and other parameters, and sets to only get file information without
 * downloading file content, to obtain remote file size.
 *
 * @param[in]  ctx_ptr FTP context pointer, containing connection required information (username, password, port, etc.)
 * @param[in]  url     FTP URL address of target file
 * @param[out] nSize   Pointer to double type, used to store obtained file size (unit: bytes)
 *
 * @return Returns QURL_OK (0) on success, returns corresponding error code on failure
 */
int unir_ftp_fota_get_file_size(unir_ftp_context_ptr ctx_ptr, const char *url, double *nSize)
{
    qurl_ecode_e res = QURL_OK;
    qurl_core_t  qurl = QOSA_NULL;
    long         filesize = 0;

    // Parameter validity check
    if (QOSA_NULL == ctx_ptr || QOSA_NULL == url || QOSA_NULL == nSize)
    {
        QLOGV("QOSA_NULL");
        return -1;
    }

    // Initialize global qurl environment and create qurl handle
    qurl_global_init();
    qurl_core_create(&ctx_ptr->qurl);
    qurl = ctx_ptr->qurl;
    if (qurl == QOSA_NULL)
    {
        res = -1;
        return res;
    }

    // Configure qurl parameters, including server port, username, password, etc.
    QLOGD("user=%s,pwd=%s,port=%d", ctx_ptr->username, ctx_ptr->password, ctx_ptr->server_port);
    qurl_core_setopt(qurl, QURL_OPT_PORT, ctx_ptr->server_port);
    qurl_core_setopt(qurl, QURL_OPT_USERNAME, ctx_ptr->username);
    qurl_core_setopt(qurl, QURL_OPT_PASSWORD, ctx_ptr->password);

    qurl_core_setopt(qurl, QURL_OPT_BOUND_THREAD_CTRL, 0L);
    qurl_core_setopt(qurl, QURL_OPT_URL, url);
    qurl_core_setopt(qurl, QURL_OPT_READ_CB, QOSA_NULL);
    qurl_core_setopt(qurl, QURL_OPT_READ_CB_ARG, (void *)QOSA_NULL);
    qurl_core_setopt(qurl, QURL_OPT_READ_HEAD_CB, QOSA_NULL);
    qurl_core_setopt(qurl, QURL_OPT_READ_HEAD_CB_ARG, QOSA_NULL);
    qurl_core_setopt(qurl, QURL_OPT_WRITE_CB, unir_ftp_request_reply);
    qurl_core_setopt(qurl, QURL_OPT_WRITE_CB_ARG, QOSA_NULL);
    qurl_core_setopt(qurl, QURL_OPT_WRITE_HEAD_CB, unir_get_ftp_repaly_code);
    qurl_core_setopt(qurl, QURL_OPT_WRITE_HEAD_CB_ARG, (void *)(ctx_ptr));
    qurl_core_setopt(qurl, QURL_OPT_TIMEOUT_MS, 0L);
    qurl_core_setopt(qurl, QURL_OPT_IDLE_TIMEOUT_MS, (ctx_ptr->timeout) * 1000);
    qurl_core_setopt(qurl, QURL_OPT_NETWORK_ID, ctx_ptr->pdp_cid);

    // Set to only get file information, not download file content
    qurl_core_setopt(qurl, QURL_OPT_NOBODY, 1L);

    // Decide whether to skip PASV mode IP address check based on configuration
    if (0 == ctx_ptr->skip_pasv_ip)
    {
        qurl_core_setopt(qurl, QURL_OPT_FTP_SKIP_PASV_IP, 0L);
    }

    // Set transfer mode to ASCII text mode (if enabled)
    if (ctx_ptr->transfer == 1)
    {
        qurl_core_setopt(qurl, QURL_OPT_TRANSFERTEXT, 1L);
    }

    // Set FTP to active or passive mode, default is passive mode
    if (ctx_ptr->ftp_port == 0)
    {
        //Default FTP operations are passive, and thus will not use PORT.
        qurl_core_setopt(qurl, QURL_OPT_FTP_PORT, "-");
    }

    // Execute FTP request to get file information
    res = qurl_core_perform(qurl);
    QLOGD("res=%x", res);

    if (QURL_OK == res)
    {
        // After successful execution, get file size
        res = qurl_core_getinfo(qurl, QURL_INFO_RESP_CONTENT_LENGTH, &filesize);
        QLOGD("res=%x,filesize=%ld", res, filesize);
        if ((QURL_OK == res) && (filesize >= 0))
        {
            QLOGD("filesize=%ld", filesize);
            *nSize = filesize;
            goto exit;
        }
    }
    else
    {
        goto exit;
    }
exit:
    // Clean up resources, delete qurl handle
    qurl_core_delete(qurl);
    return res;
}

/**
 * @brief Download file from specified URL using FTP protocol, supports resume download and callback write method
 *
 * @param ctx_ptr     FTP context pointer, containing connection required information (username, password, port, etc.)
 * @param url         FTP URL address of file to download
 * @param write_cb    Data write callback function, used to process received data
 * @param user_data   User-defined data, will be passed to write callback function
 * @param start_pos   File download start position, used for resume download functionality
 *
 * @return Returns 0 or QURL_OK on success; returns negative value or error code on failure
 */
int unir_ftp_fota_get_file(unir_ftp_context_ptr ctx_ptr, const char *url, UNIR_FTP_FOTA_WRITE_CB write_cb, void *user_data, unsigned int start_pos)
{
    qurl_ecode_e res = QURL_OK;
    qurl_core_t  qurl = QOSA_NULL;

    // Check input parameter validity
    if (QOSA_NULL == ctx_ptr || QOSA_NULL == url)
    {
        QLOGE("QOSA_NULL");
        return -1;
    }

    if (write_cb == QOSA_NULL)
    {
        QLOGE("write_cb NULL");
        return -1;
    }

    // Initialize global FTP module and create qurl handle
    qurl_global_init();
    qurl_core_create(&ctx_ptr->qurl);
    qurl = ctx_ptr->qurl;
    if (qurl == QOSA_NULL)
    {
        res = -1;
        return res;
    }

    // Configure FTP connection and transmission related parameters
    QLOGD("user=%s,pwd=%s,port=%d", ctx_ptr->username, ctx_ptr->password, ctx_ptr->server_port);
    qurl_core_setopt(qurl, QURL_OPT_PORT, ctx_ptr->server_port);
    qurl_core_setopt(qurl, QURL_OPT_USERNAME, ctx_ptr->username);
    qurl_core_setopt(qurl, QURL_OPT_PASSWORD, ctx_ptr->password);

    qurl_core_setopt(qurl, QURL_OPT_BOUND_THREAD_CTRL, 0L);
    qurl_core_setopt(qurl, QURL_OPT_URL, url);
    qurl_core_setopt(qurl, QURL_OPT_WRITE_CB, write_cb);
    qurl_core_setopt(qurl, QURL_OPT_WRITE_CB_ARG, user_data);
    qurl_core_setopt(qurl, QURL_OPT_TIMEOUT_MS, 0L);
    qurl_core_setopt(qurl, QURL_OPT_IDLE_TIMEOUT_MS, (ctx_ptr->timeout) * 1000);
    qurl_core_setopt(qurl, QURL_OPT_NETWORK_ID, ctx_ptr->pdp_cid);

    // Set whether to use text mode transmission
    if (ctx_ptr->transfer == 1)
    {
        qurl_core_setopt(qurl, QURL_OPT_TRANSFERTEXT, 1L);
    }
    // Set FTP PORT mode option, default is passive mode
    if (ctx_ptr->ftp_port == 0)
    {
        qurl_core_setopt(qurl, QURL_OPT_FTP_PORT, "-");
    }

    // If start position is set, enable resume download functionality
    if (start_pos != 0)
    {
        qurl_core_setopt(qurl, QURL_OPT_RESUME_FROM, start_pos);
    }
    // Execute FTP file download operation
    res = qurl_core_perform(qurl);

    QLOGD("res=%x", res);
    // Delete FTP handle, release resources
    qurl_core_delete(qurl);
    return res;
}

/**
 * @brief FTP FOTA upgrade process handling function
 *
 * This function implements downloading firmware upgrade package via FTP,
 * and completes the full process of local verification and device restart into upgrade mode.
 * Mainly includes network activation, FTP client initialization, file size acquisition,
 * storage space check, file download, image verification and other steps.
 *
 * @param ctx [IN] Task context pointer, unused
 * @return No return value
 */
static void unir_ftp_fota_demo_process(void *ctx)
{
    unir_fota_error_e    err = 0;
    fota_ftp_info_t      fota_ftp_info = {0};
    unir_ftp_context_ptr fota_ftp_ctxp = QOSA_NULL;
    double               file_Size = 0;
    double               free_size = 0;
    qosa_fota_errno_e    fota_result = QOSA_FOTA_OK;

    // Delay 5 seconds to prevent log loss
    qosa_task_sleep_sec(5);
    // Initialize FTP related information structure
    qosa_memset(&fota_ftp_info, 0, sizeof(fota_ftp_info_t));

    // Disable the FOTA package signature verification
    qosa_set_fota_verify_config(QOSA_FOTA_VERIFY_DISABLE);

    // Query whether the signature verification for the FOTA package is enabled
    QLOGE("fota verify config %d", qosa_get_fota_verify_config());

    // Configure SIM card ID and PDP ID
    fota_ftp_info.sim_id = 0;
    fota_ftp_info.pdp_id = 1;

    // Copy upgrade package file name and path name
    qosa_memcpy(fota_ftp_info.file_name, UNIR_FOTA_PAKET_NAME, qosa_strlen(UNIR_FOTA_PAKET_NAME) + 1);
    qosa_memcpy(fota_ftp_info.path_name, UNIR_FOTA_PATH_NAME, qosa_strlen(UNIR_FOTA_PATH_NAME) + 1);

    // Check and activate PDP network connection
    err = unir_fota_net_active(fota_ftp_info.sim_id, fota_ftp_info.pdp_id);
    if (err != UNIR_FOAT_SUCCESS)
    {
        QLOGD("qcm_file_fopen[%s]=%d", fota_ftp_info.file_name, err);
        goto exit;
    }

    // Initialize FOTA module and get handle
    fota_ftp_info.current_dl_len = 0xFFFFFFFF;
    fota_ftp_info.fota_ptr = qosa_fota_init(fota_ftp_info.file_name, QOSA_TRUE);
    if (fota_ftp_info.fota_ptr == QOSA_NULL)
    {
        QLOGD("qcm_file_fopen[%s]=%d", fota_ftp_info.file_name, err);
        goto exit;
    }
    // Set number of URC notifications during upgrade process
    qosa_fota_set_update_urc_num(fota_ftp_info.fota_ptr, 10);

    // Create FTP client handle
    fota_ftp_ctxp = unir_fota_ftp_handle_new(&fota_ftp_info);
    if (fota_ftp_ctxp == QOSA_NULL)
    {
        QLOGE("ftp init failed");
        goto exit;
    }
    // Get upgrade file size on FTP server
    err = unir_ftp_fota_get_file_size(fota_ftp_ctxp, UNIR_FOTA_URL, &file_Size);
    QLOGE("err=%d,file_Size=%d", err, file_Size);
    if (err != UNIR_FOAT_SUCCESS)
    {
        QLOGE("get ftp server file size error = %d, server file size = %d\n", err, file_Size);
        goto exit;
    }
    // Check if local storage space is sufficient for upgrade file
    fota_ftp_info.current_dl_len = file_Size;
    free_size = unirtos_fota_get_partition_space(fota_ftp_info.fota_ptr, fota_ftp_info.file_name);
    if (free_size < fota_ftp_info.current_dl_len - fota_ftp_info.current_dl_offset)
    {
        QLOGV("no space : %d,need dload : %d,current size : %d", free_size, fota_ftp_info.current_dl_len, fota_ftp_info.current_dl_offset);
        goto exit;
    }
    // Start downloading upgrade file from FTP server
    err = unir_ftp_fota_get_file(fota_ftp_ctxp, UNIR_FOTA_URL, unir_fota_ftp_transmitter_cb, &fota_ftp_info, fota_ftp_info.current_dl_offset);
    if (err != 0)
    {
        QLOGE("fota ftp dload failed,error code : %d!", fota_ftp_info.error_code);
        goto exit;
    }
exit:
    // If download successful, perform image verification
    if (err == UNIR_FOAT_SUCCESS)
    {
        fota_result = qosa_fota_verify_image(fota_ftp_info.fota_ptr);
        if (fota_result != QOSA_FOTA_OK)
        {
            QLOGV("verify failed fota_result:%d", fota_result);
        }
        else
        {
            // Verification successful, set upgrade flag and restart device into upgrade mode
            qosa_fota_flag_set();
            qosa_power_reset(QOSA_RESET_FOTA);  // Restart into upgrade
        }
    }

    // Release FOTA handle resources
    if (fota_ftp_info.fota_ptr != QOSA_NULL)
    {
        qosa_fota_deinit(fota_ftp_info.fota_ptr);
    }
    // Clean up FTP client resources
    unir_ftp_client_clean(fota_ftp_ctxp);

    return;
}

void unir_ftp_fota_demo_init(void)
{
    QLOGV("enter UniRTOS ftp fota DEMO !!!");
    if (g_unir_ftp_fota_demo_task == QOSA_NULL)
    {
        qosa_task_create(
            &g_unir_ftp_fota_demo_task,
            CONFIG_UNIRTOS_FTP_FOTA_DEMO_TASK_STACK_SIZE,
            UNIR_FTP_FOTA_DEMO_TASK_PRIO,
            "ftp_fota_demo",
            unir_ftp_fota_demo_process,
            QOSA_NULL,
            1
        );
    }
}
