/*****************************************************************/ /**
* @file websocket_demo.c
* @brief WebSocket demo implementation
* @author harry.li@quectel.com
* @date 2025-08-22
*
* @copyright Copyright (c) 2023 Quectel Wireless Solution, Co., Ltd.
* All Rights Reserved. Quectel Wireless Solution Proprietary and Confidential.
*
* @par EDIT HISTORY FOR MODULE
* <table>
* <tr><th>Date <th>Version <th>Author <th>Description
* <tr><td>2025-08-22 <td>1.0 <td>harry.li <td> Initial version
* </table>
**********************************************************************/

#include "qosa_sys.h"
#include "qosa_def.h"
#include "qosa_log.h"
#include "qcm_websocket.h"
#include "qcm_vtls_cfg.h"
#include "qosa_virtual_file.h"
#define QOS_LOG_TAG                    LOG_TAG

#define WEBSOCKET_DEMO_TASK_STACK_SIZ  4096 /*!< Web demo task stack size in bytes */

#define WEBSOCKET_DEMO_MSG_MAX_CNT     5

#define WEBSOCKET_DEMO_CONFIG_ID       0                                  /*!< Web configuration item */
#define WEBSOCKET_DEMO_CLIENT_ID       0                                  /*!< Web client */
#define WEBSOCKET_DEMO_SIMID           0                                  /*!< SIM id */
#define WEBSOCKET_DEMO_PDPID           1                                  /*!< PDP context ID for network connection */
#define WEBSOCKET_DEMO_PING_INTERVAL   10                                 /*!< Test ping interval, set shorter for observation, modify for actual use */
#define WEBSOCKET_DEMO_SEND_BUF        QCM_WEB_CFG_READ_BUF_LEN_MAX       /*!< Test send data buffer size */
#define WEBSOCKET_DEMO_WS_URL          "ws://echo.websocket.events"       /*!< If port is not default 80/443, need to include port */
#define WEBSOCKET_DEMO_WSS_URL         "wss://echo.websocket.events"      /*!< If port is not default 80/443, need to include port */

#define WEBSOCKET_DEMO_TEST_CONTENT    "Hello Welcome to test websocket!" /*!< Test send content */
#define WEBSOCKET_DEMO_FILENAME        "/user/web_test2.txt"              /*!< Test send file */

#define WEBSOCKET_DEMO_TEST_WSS        1                                  /*!< Test WS or WSS */
#define WEBSOCKET_DEMO_TEST_WSSAUTH    1                                  /*!< Test WSS TLS 0:NO VERIFY CA 1:VERIFY SERVER 2:VERIFY ALL */

#define WEBSOCKET_DEMO_TEST_CA_CERT    "cacert.pem"
#define WEBSOCKET_DEMO_TEST_CLI_CERT   "clicert.pem"
#define WEBSOCKET_DEMO_TEST_CLI_KEY    "clikey.pem"

/**
 * @brief Web demo event type enumeration definition
 */
typedef enum
{
    WEB_DEMO_EVENT_OPEN = 0,     /*!< Connection open event */
    WEB_DEMO_EVENT_RECV = 1,     /*!< Data receive event */
    WEB_DEMO_EVENT_WRITE = 2,    /*!< Data write event */
    WEB_DEMO_EVENT_CLOSEING = 3, /*!< Connection closing event */
    WEB_DEMO_EVENT_CLOSED = 4,   /*!< Connection closed event */
} web_demo_event_e;

/**
 * @brief WebSocket connection status enumeration definition
 *
 * Defines various WebSocket connection states for tracking connection lifecycle
 */
typedef enum
{
    WEB_DEMO_STATUS_INIT = 0,                          /*!< Initialization state */
    WEB_DEMO_STATUS_CONNECTING = 1,                    /*!< Connecting */
    WEB_DEMO_STATUS_CONNECTED = 2,                     /*!< WebSocket connected */
    WEB_DEMO_STATUS_TCP_CLOSING = 3,                   /*!< Closing state, indicating connection is closing */
    WEB_DEMO_STATUS_TCP_CLOSED = WEB_DEMO_STATUS_INIT, /*!< Closed state, indicating connection has been closed */
} web_demo_status_e;

/**
 * @brief web_demo_msg_t structure definition
 *
 * @param msg_id Event ID, used to identify different event types
 * @param param1 Data parameter 1, used to pass integer data
 * @param argv Data parameter 2, used to pass pointer type data
 */
typedef struct
{
    int   msg_id; /*!< Event ID */
    int   param1; /*!< Data */
    void *argv;   /*!< Data */
} web_demo_msg_t;

/**
 * @brief Web demo data structure, used to manage file upload and read operation related information
 */
typedef struct
{
    int   is_file;   /*!< File upload */
    int   total_len; /*!< Total file size */
    int   used_len;  /*!< Already read file size */
    int   fd;        /*!< File handle */
    char *buf;       /*!< Read data */
    int   buf_len;   /*!< Read data length */
    int   complete;  /*!< Test complete */
} web_demo_data_t;

/**
 * @brief Web section information
 */
typedef struct
{
    web_demo_status_e states; /*!< Web connection status */
    web_demo_data_t   data;   /*!< Web file data */
    qosa_msgq_t       msgq;   /*!< Message queue */
} web_demo_ctx_t;

// Client context
static web_demo_ctx_t g_web_ctx = {0};

/*===========================================================================
 *  Functions
 ===========================================================================*/

/**
 * @brief Web service callback function, handles various events from Web module.
 *
 * This function handles connection open, close, data receive and writable events,
 * and notifies the upper application of status changes or data arrival through message queue.
 *
 * @param client_id Client ID, identifies the current Web connection client
 * @param cb_t      Callback information structure pointer, contains event type and related parameters
 */

static int quec_web_demo_service_cb(int client_id, qcm_web_cb_t *cb_t)
{
    // Initialize message structure
    web_demo_msg_t msg = {0};

    QLOGD("type=%d", cb_t->type);
    // Process based on callback event type
    switch (cb_t->type)
    {
        case QCM_WEB_TYPE_OPEN:
            // Handle connection open event
            QLOGD("result=%d", cb_t->result);
            if (cb_t->result == 0)
            {
                g_web_ctx.states = WEB_DEMO_STATUS_CONNECTED;
            }
            else
            {
                g_web_ctx.states = WEB_DEMO_STATUS_INIT;
            }
            msg.msg_id = WEB_DEMO_EVENT_OPEN;
            qosa_msgq_release(g_web_ctx.msgq, sizeof(web_demo_msg_t), (qosa_uint8_t *)&msg, QOSA_NO_WAIT);
            break;
        case QCM_WEB_TYPE_CLOSE:
            // Handle connection close event
            g_web_ctx.states = WEB_DEMO_STATUS_TCP_CLOSED;
            msg.msg_id = WEB_DEMO_EVENT_CLOSED;
            qosa_msgq_release(g_web_ctx.msgq, sizeof(web_demo_msg_t), (qosa_uint8_t *)&msg, QOSA_NO_WAIT);
            break;
        case QCM_WEB_TYPE_RECV:
            // Handle data receive event
            msg.msg_id = WEB_DEMO_EVENT_RECV;
            msg.param1 = cb_t->size;
            qosa_msgq_release(g_web_ctx.msgq, sizeof(web_demo_msg_t), (qosa_uint8_t *)&msg, QOSA_NO_WAIT);
            break;

        case QCM_WEB_TYPE_WRITE:
            // Handle continue write event (last send buffer full, now can continue sending)
            msg.msg_id = WEB_DEMO_EVENT_CLOSEING;
            qosa_msgq_release(g_web_ctx.msgq, sizeof(web_demo_msg_t), (qosa_uint8_t *)&msg, QOSA_NO_WAIT);
            break;
        default:
            // Unknown event type, do nothing
            break;
    }

    // Return processing success status
    return 0;
}

/**
 * @brief Initialize Web configuration parameters
 *
 * This function is used to initialize Web configuration parameters for the specified configuration ID,
 * including setting SIM card ID, PDP context ID, connection URL, event callback function,
 * and PING interval parameters.
 *
 * @param config_id Configuration ID, used to identify specific Web configuration instance
 */
static void quec_web_demo_cfg_init(int config_id, qcm_ssl_config_t *ssl_config)
{
    qcm_web_config_t *cfg_ptr = QOSA_NULL;
    int               ret = 0;
    // Here demonstrate two configuration methods, one is to configure individual items separately,
    // the other is to get all configuration items and then configure the structure
    // Get configuration pointer and set connection parameters
    cfg_ptr = qcm_ws_cfg_get_all(config_id);
    cfg_ptr->conn.sim_cid = WEBSOCKET_DEMO_SIMID;
    cfg_ptr->conn.pdp_cid = WEBSOCKET_DEMO_PDPID;

    // Configure connection URL separately
    if (WEBSOCKET_DEMO_TEST_WSS == 0)
    {
        // Test ws connection
        ret = qcm_ws_cfg_set(config_id, QCM_WEB_CFG_CONN_URL, (void *)WEBSOCKET_DEMO_WS_URL, qosa_strlen(WEBSOCKET_DEMO_WS_URL));
    }
    else
    {
        // Test wss connection
        ret = qcm_ws_cfg_set(config_id, QCM_WEB_CFG_CONN_URL, (void *)WEBSOCKET_DEMO_WSS_URL, qosa_strlen(WEBSOCKET_DEMO_WSS_URL));
        // Configure TLS connection related configuration information
        qosa_memset(ssl_config, 0x00, sizeof(qcm_ssl_config_t));
        ssl_config->ssl_version = QCM_SSL_VERSION_3;   // tls1.2
        ssl_config->transport = QCM_SSL_TLS_PROTOCOL;  // tls
        ssl_config->ssl_negotiate_timeout = 30;        // TLS handshake timeout
        ssl_config->ssl_log_debug = 4;                 // Enable TLS log output option for debugging
        ssl_config->sni_enable = QOSA_TRUE;            // Enable SNI
        if(WEBSOCKET_DEMO_TEST_WSSAUTH == 0)
        {
            ssl_config->auth_mode = QCM_SSL_VERIFY_NULL;   // No authentication
        }
        else
        {
            if(WEBSOCKET_DEMO_TEST_WSSAUTH == 1)
            {
                ssl_config->auth_mode = QCM_SSL_VERIFY_SERVER;   // No authentication
            }
            else
            {
                ssl_config->auth_mode = QCM_SSL_VERIFY_CLIENT_SERVER;   // No authentication
            }        
            // The following example configures a certificate. 
            // You need to ensure that the certificate exists in the file system and configure it according to the actual name.
            ssl_config->ca_cert_path[0] = qosa_malloc(qosa_strlen(WEBSOCKET_DEMO_TEST_CA_CERT)+1);
            ssl_config->own_cert_path = qosa_malloc(qosa_strlen(WEBSOCKET_DEMO_TEST_CLI_CERT)+1);
            ssl_config->own_key_path = qosa_malloc(qosa_strlen(WEBSOCKET_DEMO_TEST_CLI_KEY)+1);
            qosa_memset(ssl_config->ca_cert_path[0], 0, qosa_strlen(WEBSOCKET_DEMO_TEST_CA_CERT)+1);
            qosa_memset(ssl_config->own_cert_path, 0, qosa_strlen(WEBSOCKET_DEMO_TEST_CLI_CERT)+1);
            qosa_memset(ssl_config->own_key_path, 0, qosa_strlen(WEBSOCKET_DEMO_TEST_CLI_KEY)+1);
            qosa_memcpy(ssl_config->ca_cert_path[0], WEBSOCKET_DEMO_TEST_CA_CERT, qosa_strlen(WEBSOCKET_DEMO_TEST_CA_CERT));
            qosa_memcpy(ssl_config->own_cert_path, WEBSOCKET_DEMO_TEST_CLI_CERT, qosa_strlen(WEBSOCKET_DEMO_TEST_CLI_CERT));
            qosa_memcpy(ssl_config->own_key_path, WEBSOCKET_DEMO_TEST_CLI_KEY, qosa_strlen(WEBSOCKET_DEMO_TEST_CLI_KEY));
        }
        qcm_ws_cfg_set(config_id, QCM_WEB_CFG_CONN_SSLCONFIG, (void *)&ssl_config, 0);
    }    
    // Configure Web event callback function
    ret = qcm_ws_cfg_set(config_id, QCM_WEB_CFG_RECV_CB, (void *)quec_web_demo_service_cb, 0);
    // Configure PING interval time, set to 0 to disable PING
    ret = qcm_ws_cfg_set(config_id, QCM_WEB_CFG_PING_INTERVAL, QOSA_NULL, WEBSOCKET_DEMO_PING_INTERVAL);
    // Configure maximum send buffer size
    ret = qcm_ws_cfg_set(config_id, QCM_WEB_CFG_WRITE_BUFFERSZ, QOSA_NULL, WEBSOCKET_DEMO_SEND_BUF);
    QLOGD("ret=%x", ret);
}

/**
 * @brief Open WebSocket connection
 */
static void quec_web_demo_open(qcm_ssl_config_t *ssl_config)
{
    /* Initialize return value and open parameter structure */
    int            ret = 0;
    qcm_web_open_t open = {0};

    /* Initialize WebSocket configuration */
    quec_web_demo_cfg_init(WEBSOCKET_DEMO_CONFIG_ID, ssl_config);

    /* Set client ID and configuration ID */
    open.client_id = WEBSOCKET_DEMO_CLIENT_ID;
    open.config_id = WEBSOCKET_DEMO_CONFIG_ID;

    /* Call WebSocket open processing function */
    ret = qcm_ws_open_proc(&open);
    if (ret != QCM_WEB_ERR_OK)
    {
        QLOGE("error");
        return;
    }
}
/**
 * @brief Close WebSocket connection
 */
static void quec_web_demo_close(void)
{
    qcm_ws_close_proc(WEBSOCKET_DEMO_CLIENT_ID);
}

/**
 * @brief Read specified length data from WebSocket client and perform test
 *
 * @param client_id Client connection identifier
 * @param size Data length to read
 */
static void quec_web_demo_read_test(int client_id, int size)
{
    char *read_data = QOSA_NULL;  // Read data buffer
    int   read_len = size;   // Expected read length
    int   ret = 0;

    QLOGD("read_len=%d", read_len);
    // Allocate memory space for storing read data
    read_data = qosa_malloc(read_len + 1);
    if (read_data == QOSA_NULL)
    {
        return;
    }
    qosa_memset(read_data, 0x00, read_len + 1);

    // Read data from client
    ret = qcm_ws_read_proc(client_id, read_data, &read_len);
    QLOGD("ret=%x,read_len=%d", ret, read_len);
    QLOGD("read_data=%s", read_data);
    // If data read fails, free memory and return
    if (ret != 0)
    {
        qosa_free(read_data);
        return;
    }

    // Free data
    qosa_free(read_data);
}
/**
 * Read specified length data from file descriptor to buffer
 *
 * @param fd File descriptor
 * @param buf Data read buffer pointer
 * @param buf_len Data length to read
 * @return Returns 0 on success, -1 on failure
 *
 * This function ensures complete data reading by calling qosa_vfs_read in a loop
 */
static int quec_web_file_read(int fd, char *buf, int buf_len)
{
    int ret = 0;
    int temp_len = 0;

    /* Loop read data until specified length is reached or error occurs */
    while (temp_len < buf_len)
    {
        ret = qosa_vfs_read(fd, buf + temp_len, buf_len - temp_len);
        if (ret < 0)
        {
            QLOGD("ret=%x", ret);
            return -1;
        }
        temp_len += ret;
    }
    return 0;
}
/**
 * @brief WebSocket write test function, used to send text or file content to client
 *
 * This function first attempts to send a predefined test text. If successful,
 * it continues to try to open and send the content of a file.
 * File content will be read in chunks and sent through WebSocket until the entire file transfer is complete.
 *
 * @note Web has single send limit, currently files also need to be sent in multiple segments
 */
static void quec_web_demo_write_test(void)
{
    int                    ret = 0;
    int                    len = 0;
    web_demo_data_t       *data = QOSA_NULL;
    int                    free_len = 0;
    struct qosa_vfs_stat_t stat = {0};

    QLOGV("write test");
    data = &g_web_ctx.data;
    // If test text hasn't been sent yet, send text content first
    if (data->is_file == 0)
    {
        len = qosa_strlen(WEBSOCKET_DEMO_TEST_CONTENT);
        ret = qcm_ws_write_proc(WEBSOCKET_DEMO_CLIENT_ID, WEBSOCKET_DEMO_TEST_CONTENT, len, QCM_WEB_OPCODE_TEXT);
        data->is_file = 1;
    }

    // Ensure buffer is allocated
    if (data->buf == QOSA_NULL)
    {
        data->buf = qosa_malloc(WEBSOCKET_DEMO_SEND_BUF + 1);
        if (data->buf == QOSA_NULL)
        {
            QLOGE("malloc error");
            return;
        }
    }
    // Send file content
    if (data->is_file == 1)
    {
        while (1)
        {
            QLOGD("total_len=%d,used_len=%d", data->total_len, data->used_len);

            // Open file and get file information
            if (data->fd <= 0)
            {
                // Open file
                data->fd = qosa_vfs_open(WEBSOCKET_DEMO_FILENAME, QOSA_VFS_O_RDONLY);
                if (data->fd < 0)
                {
                    QLOGE("open file error");
                    goto exit;
                }
                // Get file size
                ret = qosa_vfs_fstat(data->fd, &stat);
                if (ret != 0)
                {
                    /* Print file size */
                    QLOGE("size error");
                    goto exit;
                }
                data->total_len = stat.st_size;
                // Move file pointer to beginning of file
                qosa_vfs_lseek(data->fd, 0, QOSA_VFS_SEEK_SET);
            }

            // If there is data in buffer waiting to be sent, send buffer data first
            if (data->buf_len > 0)
            {
                // Send last unsuccessfully sent data
                ret = qcm_ws_write_proc(WEBSOCKET_DEMO_CLIENT_ID, data->buf, data->buf_len, QCM_WEB_OPCODE_TEXT);
                if (ret == QCM_WEB_ERR_OK)
                {
                    data->buf_len = 0;
                }
                else if (ret == QCM_WEB_ERR_NOMEM)
                {
                    // Send buffer full, wait for write event to continue sending
                    QLOGV("buffer wait");
                    break;
                }
                else
                {
                    QLOGE("send error=%x", ret);
                    goto exit;
                }
            }

            // Continue reading file and sending content
            if (data->used_len < data->total_len)
            {
                free_len = data->total_len - data->used_len;
                // Get length to send
                data->buf_len = MIN(free_len, WEBSOCKET_DEMO_SEND_BUF);
                qosa_memset(data->buf, 0, WEBSOCKET_DEMO_SEND_BUF + 1);
                // Read data from file
                ret = quec_web_file_read(data->fd, data->buf, data->buf_len);
                if (ret < 0)
                {
                    QLOGE("file read error");
                    goto exit;
                }
                data->used_len += data->buf_len;
                if (data->used_len >= data->total_len)
                {
                    QLOGV("read file complete");
                }
                // Send data
                ret = qcm_ws_write_proc(WEBSOCKET_DEMO_CLIENT_ID, data->buf, data->buf_len, QCM_WEB_OPCODE_TEXT);
                if (ret == QCM_WEB_ERR_OK)
                {
                    data->buf_len = 0;
                    continue;
                }
                else if (ret == QCM_WEB_ERR_NOMEM)
                {
                    // Send buffer full, wait for write event to continue sending
                    break;
                }
                else
                {
                    QLOGE("send error=%x", ret);
                    goto exit;
                }
            }
            else
            {
                QLOGV("send complete");
                goto exit;
            }
        }
    }

    QLOGD("ret=%x,len=%d", ret, len);
    return;

exit:
    // Clean up resources: close file descriptor and free buffer memory
    if (data->fd > 0)
    {
        qosa_vfs_close(data->fd);
        data->fd = 0;
    }
    if (data->buf != QOSA_NULL)
    {
        qosa_free(data->buf);
        data->buf = QOSA_NULL;
    }
    data->complete = 1;
}

/**
 * @brief WebSocket application initialization and main processing function
 *
 * This function is responsible for initializing the WebSocket application environment, creating message queue,
 * starting WebSocket connection, and looping to handle various event messages, including connection,
 * data reception, data sending, and connection close operations.
 */
static void quec_web_demo_app_init(void *argv)
{
    int              ret = 0;
    web_demo_msg_t   msg = {0};
    qcm_ssl_config_t ssl_config = {0};

    // Wait for system initialization to complete
    qosa_task_sleep_sec(10);
    // Create message queue for handling WebSocket related events
    qosa_msgq_create(&g_web_ctx.msgq, sizeof(web_demo_msg_t), WEBSOCKET_DEMO_MSG_MAX_CNT);
    // Establish WebSocket connection
    quec_web_demo_open(&ssl_config);

    // Main event processing loop
    while (1)
    {
        // Wait for event messages in message queue
        ret = qosa_msgq_wait(g_web_ctx.msgq, (qosa_uint8_t *)&msg, sizeof(web_demo_msg_t), QOSA_WAIT_FOREVER);
        if (ret != QOSA_OK)
        {
            continue;
        }

        // Handle different events based on message ID
        switch (msg.msg_id)
        {
            // Handle connection open event
            case WEB_DEMO_EVENT_OPEN: {
                QLOGD("status=%d", g_web_ctx.states);
                // If connection is established, start write test
                if (g_web_ctx.states == WEB_DEMO_STATUS_CONNECTED)
                {
                    quec_web_demo_write_test();
                }
            }
            break;
            // Handle data receive event
            case WEB_DEMO_EVENT_RECV: {
                quec_web_demo_read_test(WEBSOCKET_DEMO_CLIENT_ID, msg.param1);
            }
            break;
            // Handle data write event
            case WEB_DEMO_EVENT_WRITE: {
                quec_web_demo_write_test();
            }
            break;
            // Handle connection close event
            case WEB_DEMO_EVENT_CLOSEING: {
                quec_web_demo_close();
                QLOGV("closing");
            }
            break;
            // Handle connection close event
            case WEB_DEMO_EVENT_CLOSED: {
                QLOGV("closed");
                goto exit;
            }
            break;
        }
        // Check if data transfer is complete
        if (g_web_ctx.data.complete == 1)
        {
            // Wait a few seconds before closing test
            qosa_task_sleep_sec(5);
            msg.msg_id = WEB_DEMO_EVENT_CLOSEING;
            qosa_msgq_release(g_web_ctx.msgq, sizeof(web_demo_msg_t), (qosa_uint8_t *)&msg, QOSA_NO_WAIT);
        }
    }

exit:
    // Release resources
    qosa_msgq_delete(g_web_ctx.msgq);
    g_web_ctx.msgq = QOSA_NULL;
    if (g_web_ctx.data.fd > 0)
    {
        qosa_vfs_close(g_web_ctx.data.fd);
    }
    if (g_web_ctx.data.buf != QOSA_NULL)
    {
        qosa_free(g_web_ctx.data.buf);
    }
    if(ssl_config->ca_cert_path[0])
    {
        qosa_free(ssl_config->ca_cert_path[0]);
    }
    if(ssl_config->own_cert_path)
    {
        qosa_free(ssl_config->own_cert_path);
    }
    if(ssl_config->own_key_path)
    {
        qosa_free(ssl_config->own_key_path);
    }
    QLOGV("end");
}
void quec_web_demo_init(void)
{
    int         err = 0;
    qosa_task_t web_task = QOSA_NULL;

    err = qosa_task_create(&web_task, WEBSOCKET_DEMO_TASK_STACK_SIZE, QOSA_PRIORITY_NORMAL, "web_demo", quec_web_demo_app_init, QOSA_NULL);
    if (err != QOSA_OK)
    {
        QLOGE("task create error");
        return;
    }
}