/*****************************************************************/ /**
* @file qcm_socket_noblock_demo.c
* @brief
* @author harry.li@quectel.com
* @date 2025-05-7
*
* @copyright Copyright (c) 2023 Quectel Wireless Solution, Co., Ltd.
* All Rights Reserved. Quectel Wireless Solution Proprietary and Confidential.
*
* @par EDIT HISTORY FOR MODULE
* <table>
* <tr><th>Date <th>Version <th>Author <th>Description
* <tr><td>2025-05-7 <td>1.0 <td>harry.li <td> Init
* </table>
**********************************************************************/
#include "qcm_socket_adp.h"
#include "qosa_asyn_dns.h"
#include "qosa_buffer_block.h"
#include "qosa_datacall.h"
#include "qosa_def.h"
#include "qosa_log.h"
#include "qosa_sockets.h"
#include "qosa_sys.h"
#include "qosa_watermark.h"

#define QOS_LOG_TAG                         LOG_TAG
#define SOCKET_NOBLOCK_DEMO_TASK_STACK_SIZE 4096
#define SOCKET_NOBLOCK_DEMO_TASK_PRIO       QOSA_PRIORITY_NORMAL

// Non-blocking connection server IP address
#define SOCKET_NOBLOCK_CONNECT_SERVER_ADDR  "101.37.104.185"
// Non-blocking connection server port number
#define SOCKET_NOBLOCK_CONNECT_SERVER_PORT  47008
// Non-blocking connection server domain name
#define SOCKET_NOBLOCK_CONNECT_SERVER_NAME  "101.37.104.185"
// Maximum length of receive buffer
#define RECV_BUFF_MAX_LEN                   1500

// SIM card and PDP index for PDP activation
#define SOCKET_BLOCK_DEMO_SIMID             0
#define SOCKET_BLOCK_DEMO_PDPID             1
// PDP activation timeout 30s
#define SOCKET_BLOCK_DEMO_ACTIVE_TIMEOUT    30

/** TCPUDP single send data size, TCP MTU is usually 1350-1460, 1500 can trigger TCPIP protocol stack packet sending */
#define SOCKET_APP_SINGLE_SEND_LEN          1500
/** TCPUDP single receive data length size */
#define SOCKET_APP_SINGLE_READ_LEN          1500

/** TCPUDP receive/send Watermark maximum cache space */
#define SOCKET_APP_WATERMARK_LIMIT          10240
#define SOCKET_APP_WATERMARK_LOW_LIMIT      2048
#define SOCKET_APP_WATERMARK_HIGH_LIMIT     8192

/**
 * @brief Socket application message type enumeration definition
 *
 * This enumeration defines non-blocking message types used in socket applications,
 * used to identify different socket events and operation indications.
 */
typedef enum
{
    SOCKET_APP_NOBLOCK_MSG_MIN = 0,       /*!< Message type minimum value */
    SOCKET_APP_NOBLOCK_MSG_EVENT_IND = 1, /*!< Socket event notification */
    SOCKET_APP_NOBLOCK_MSG_SEND_IND,      /*!< Send indication message */
    SOCKET_APP_NOBLOCK_MSG_CLOSE,         /*!< Close socket message */
    SOCKET_APP_NOBLOCK_MSG_MAX            /*!< Message type maximum value */
} socket_app_msg_type_e;

/**
 * @enum socket_app_conn_status_e
 * @brief Socket APP application layer management socket connection status
 */
typedef enum
{
    SOCKET_APP_CONNECT_STATE_IDLE = 0,                               /*!< Socket not performing any operation */
    SOCKET_APP_CONNECT_STATE_INIT = 1,                               /*!< Socket initialization */
    SOCKET_APP_CONNECT_STATE_CONNECTING = 2,                         /*!< Socket connecting */
    SOCKET_APP_CONNECT_STATE_CONNECT = 3,                            /*!< Socket connection successful */
    SOCKET_APP_CONNECT_STATE_CLOSING = 4,                            /*!< Socket connection closing, preparing to close */
    SOCKET_APP_CONNECT_STATE_CLOSED = SOCKET_APP_CONNECT_STATE_IDLE, /*!< Socket closed and resources released */
} socket_app_conn_status_e;

/**
 * @brief Socket application message information structure
 */
typedef struct
{
    socket_app_msg_type_e event_type; /*!< Message event type */
    void                 *argv;       /*!< Message parameter pointer, points to specific parameter data */
} socket_app_msg_info_t;

/**
 * @brief Socket application event indication structure
 *
 * This structure is used to encapsulate socket-related event information, including socket file descriptor,
 * event mask, result code, and user-defined parameters
 */
typedef struct
{
    int   sockfd;      /*!< Socket file descriptor */
    int   event_mask;  /*!< Event mask, identifies the type of event that occurred */
    int   result_code; /*!< Operation result code, indicates the execution result of the operation */
    void *argv;        /*!< User-defined parameter pointer, used to pass additional data */
} socket_app_event_ind_t;

/**
 * @strcut socket_app_transfer_session
 * @brief Socket data transfer session structure
 */
typedef struct
{
    qosa_bool_t             rx_tx_ready;           /*!< Watermark initialization flag */
    qosa_bool_t             send_wm_high;          /*!< Send watermark high status flag */
    qosa_buffer_block_t    *current_send_data_ptr; /*!< Current item already taken out from Watermark */
    qosa_buffer_watermark_t send_wm_ptr;           /*!< Send watermark */
    qosa_q_type_t           send_q;                /*!< Watermark internal management queue */
    qosa_bool_t             recv_wm_high;          /*!< Receive watermark high status flag */
    qosa_buffer_block_t    *current_recv_data_ptr; /*!< Current item already taken out from Watermark */
    qosa_buffer_watermark_t recv_wm_ptr;           /*!< Receive watermark */
    qosa_q_type_t           recv_q;                /*!< Watermark internal management queue */
} socket_app_transfer_session;

/**
 * @struct socket_app_context_t
 * @brief Socket APP context structure
 */
typedef struct
{
    char                        hostname[256];      /*!< Remote host address */
    int                         hostport;           /*!< Remote host port number */
    int                         sockfd;             /*!< Internally created socket handle information */
    socket_app_conn_status_e    connect_state;      /*!< Socket connection status */
    int                         send_release_count; /*!< Count of send release events */
    socket_app_transfer_session transfer_session;   /*!< Socket app data transfer watermark */
} socket_app_context_t;

/*===========================================================================
 *  Variate
 ===========================================================================*/

/**
 * @brief Socket event message queue
 */
static qosa_msgq_t g_socket_msgq;
// Socket APP context
static socket_app_context_t g_socket_context;

/*===========================================================================
 *
 ===========================================================================*/

/**
 * @brief Check and activate PDP
 *
 * This function is used to check the data connection status of the specified SIM card and PDP context,
 * and perform synchronous activation if not activated.
 * Uses predefined SIMID and PDPID parameters to establish data connection.
 *
 * @return int Execution result
 * @retval 0  Successfully activated data connection or connection is already active
 * @retval -1 Failed to activate data connection
 */
static int qcm_socket_app_datacall_active(void)
{
    /* Define data connection related variables */
    qosa_datacall_conn_t    conn = 0;
    qosa_datacall_ip_info_t info = {0};
    qosa_datacall_errno_e   ret = 0;
    qosa_pdp_context_t      pdp_ctx = {0};
    char                    ip4addr_buf[CONFIG_QOSA_INET_ADDRSTRLEN] = {0};
    char                    ip6addr_buf[CONFIG_QOSA_INET6_ADDRSTRLEN] = {0};

    const char *apn_str = "CTNET";
    pdp_ctx.apn_valid = QOSA_TRUE;
    pdp_ctx.pdp_type = QOSA_PDP_TYPE_IP;  //ipv4
    if (pdp_ctx.apn_valid)
    {
        qosa_memcpy(pdp_ctx.apn, apn_str, qosa_strlen(apn_str));
    }
    ret = qosa_datacall_set_pdp_context(SOCKET_BLOCK_DEMO_SIMID, SOCKET_BLOCK_DEMO_PDPID, &pdp_ctx);
    QLOGI("set pdp context, ret=%d", ret);

    /* Create new data connection object */
    conn = qosa_datacall_conn_new(SOCKET_BLOCK_DEMO_SIMID, SOCKET_BLOCK_DEMO_PDPID, QOSA_DATACALL_CONN_TCPIP);

    /* Check data connection information to determine if PDP is activated */
    if (QOSA_DATACALL_ERR_NO_ACTIVE == qosa_datacall_get_ip_info(conn, &info))
    {
        QLOGD("sim_id=%d,pdp_id=%d", SOCKET_BLOCK_DEMO_SIMID, SOCKET_BLOCK_DEMO_PDPID);
        // If PDP is not activated, start synchronous activation process
        ret = qosa_datacall_start(conn, SOCKET_BLOCK_DEMO_ACTIVE_TIMEOUT);
        if (QOSA_DATACALL_OK != ret)
        {
            QLOGD("datacall ret=%x", ret);
            return -1;
        }
        // Get IP info from datacall
        qosa_datacall_get_ip_info(conn, &info);
        QLOGI("ip_type=%d", info.ip_type);

        if (info.ip_type == QOSA_PDP_IPV4)
        {
            // IPv4 info
            qosa_memset(ip4addr_buf, 0, sizeof(ip4addr_buf));
            qosa_ip_addr_inet_ntop(QOSA_IP_ADDR_AF_INET, &info.ipv4_ip.addr.ipv4_addr, ip4addr_buf, sizeof(ip4addr_buf));
            QLOGI("ipv4 addr:%s", ip4addr_buf);
        }
        else if (info.ip_type == QOSA_PDP_IPV6)
        {
            // IPv6 info
            qosa_task_sleep_sec(10);
            qosa_memset(ip6addr_buf, 0, sizeof(ip6addr_buf));
            qosa_ip_addr_inet_ntop(QOSA_IP_ADDR_AF_INET6, &info.ipv6_ip.addr.ipv6_addr, ip6addr_buf, sizeof(ip6addr_buf));
            QLOGI("ipv6 addr:%s", ip6addr_buf);
        }
        else
        {
            // IPv4 and IPv6 info
            qosa_memset(ip4addr_buf, 0, sizeof(ip4addr_buf));
            qosa_ip_addr_inet_ntop(QOSA_IP_ADDR_AF_INET, &info.ipv4_ip.addr.ipv4_addr, ip4addr_buf, sizeof(ip4addr_buf));
            QLOGI("ipv4 addr:%s", ip4addr_buf);
            qosa_memset(ip6addr_buf, 0, sizeof(ip6addr_buf));
            qosa_ip_addr_inet_ntop(QOSA_IP_ADDR_AF_INET6, &info.ipv6_ip.addr.ipv6_addr, ip6addr_buf, sizeof(ip6addr_buf));
            QLOGI("ipv6 addr:%s", ip6addr_buf);
        }
    }
    return 0;
}

/**
 * @brief Send socket application event report to message queue
 *
 * @param cmd Event type command code
 * @param argv Event-related parameter pointer
 *
 * This function encapsulates the specified event type and parameters into a message structure,
 * then sends it out through the message queue.
 */
static void socket_app_event_report(socket_app_msg_type_e cmd, void *argv)
{
    /* Define message information structure and return value variable */
    socket_app_msg_info_t msg_info;
    int                   ret = 0;

    /* Initialize message structure and fill event information */
    qosa_memset(&msg_info, 0, sizeof(socket_app_msg_info_t));
    msg_info.event_type = cmd;
    msg_info.argv = argv;

    /* Send message to global socket message queue */
    ret = qosa_msgq_release(g_socket_msgq, sizeof(socket_app_msg_info_t), (qosa_uint8_t *)&msg_info, QOSA_NO_WAIT);
    if (ret != QOSA_OK)
    {
        /* Print error information when sending fails */
        QLOGE("qosa_msgq_send error");
    }
}

/**
 * @brief Socket underlying event callback function, used to handle socket events and report
 *
 * @param sockfd Socket file descriptor
 * @param event_mask Event mask, identifies the type of event that occurred
 * @param result Operation result code
 * @param argv Additional parameter pointer
 */
static void socket_app_register_event_cb(int sockfd, int event_mask, int result, void *argv)
{
    socket_app_event_ind_t *ind_msg = QOSA_NULL;

    /* Allocate event indication message memory */
    ind_msg = qosa_malloc(sizeof(socket_app_event_ind_t));
    if (ind_msg == QOSA_NULL)
    {
        QLOGE("qosa_malloc error");
        return;
    }

    /* Fill event indication message structure */
    ind_msg->sockfd = sockfd;
    ind_msg->event_mask = event_mask;
    ind_msg->result_code = result;
    ind_msg->argv = argv;

    /* Report socket application event */
    socket_app_event_report(SOCKET_APP_NOBLOCK_MSG_EVENT_IND, ind_msg);
}

/**
 * @brief TX data send watermark ring buffer remaining count below low water level
 *
 * Triggered when send buffer water level drops to low water level, used to notify upper layer to continue sending data
 *
 * @param wm_ptr Watermark information pointer (unused)
 * @param callback_data Callback data pointer, points to socket application context
 */
static void socket_app_tx_wm_low_level_cb(qosa_buffer_watermark_t *wm_ptr, void *callback_data)
{
    QOSA_UNUSED(wm_ptr);
    QLOGV("...");

    // Get socket application context and update send water level status
    socket_app_context_t *context = (socket_app_context_t *)callback_data;
    context->transfer_session.send_wm_high = QOSA_FALSE;

    // Watermark data is log water level, upper layer can continue sending data
}

/**
 * @brief TX data send watermark ring buffer remaining count above high water level
 *
 * Called when send buffer water level reaches high water level, used to notify upper layer application to stop sending data
 *
 * @param wm_ptr Watermark information pointer (unused)
 * @param callback_data Callback data pointer, points to socket application context
 */
static void socket_app_tx_wm_high_level_cb(qosa_buffer_watermark_t *wm_ptr, void *callback_data)
{
    QOSA_UNUSED(wm_ptr);
    QLOGV("...");
    socket_app_context_t *context = (socket_app_context_t *)callback_data;
    context->transfer_session.send_wm_high = QOSA_TRUE;
    // Current watermark is high state, upper layer should stop sending data
}

/**
 * @brief TX starts to enter data, watermark space changes from 0 data length to greater than 0 data length
 *
 * Called when send buffer watermark state becomes non-empty, used to handle send buffer state changes
 *
 * @param wm_ptr Watermark state pointer, points to current watermark state information
 * @param callback_data Callback data pointer, points to socket application context information
 */
static void socket_app_tx_wm_non_empty_cb(qosa_buffer_watermark_t *wm_ptr, void *callback_data)
{
    QOSA_UNUSED(wm_ptr);
    QLOGV("...");

    // Get application context and update send release count
    socket_app_context_t *context = (socket_app_context_t *)callback_data;
    context->send_release_count++;

    // Report non-blocking message send event
    socket_app_event_report(SOCKET_APP_NOBLOCK_MSG_SEND_IND, callback_data);
}

/**
 * @brief RX data send watermark ring buffer remaining count below low water level
 *
 * Called when receive buffer water level drops to preset threshold, used to control data send flow
 *
 * @param wm_ptr Watermark information pointer (unused)
 * @param callback_data Callback data pointer, points to socket application context
 */
static void socket_app_rx_wm_low_level_cb(qosa_buffer_watermark_t *wm_ptr, void *callback_data)
{
    QOSA_UNUSED(wm_ptr);
    QLOGV("...");
    socket_app_context_t *context = (socket_app_context_t *)callback_data;

    context->transfer_session.send_wm_high = QOSA_FALSE;
    // Watermark data is log water level, upper layer should continue sending data
}

/**
 * @brief RX data send watermark ring buffer remaining count above high water level
 *
 * Triggered when receive buffer water level reaches high water level, used to notify upper layer application
 * to stop sending data, prevent buffer overflow
 *
 * @param wm_ptr Watermark information pointer (unused)
 * @param callback_data Callback data pointer, points to socket application context
 */
static void socket_app_rx_wm_high_level_cb(qosa_buffer_watermark_t *wm_ptr, void *callback_data)
{
    QOSA_UNUSED(wm_ptr);
    QLOGV("...");
    socket_app_context_t *context = (socket_app_context_t *)callback_data;

    context->transfer_session.send_wm_high = QOSA_TRUE;
    // Current watermark is high state, upper layer should stop sending data
    // User should add event notification function here, notify send module to consider exception logic or stop sending data,
    // wait for watermark to become low state to continue sending
}

/**
 * @brief RX starts to enter data, watermark space changes from 0 data length to greater than 0 data length
 *
 * Called when receive buffer water level changes from empty state to non-empty state
 *
 * @param wm_ptr Pointer to water level mark structure
 * @param callback_data Callback function additional data pointer
 */
static void socket_app_rx_wm_non_empty_cb(qosa_buffer_watermark_t *wm_ptr, void *callback_data)
{
    // Mark parameters as unused to avoid compiler warnings
    QOSA_UNUSED(wm_ptr);
    QOSA_UNUSED(callback_data);

    // Print debug information
    QLOGV("...");
}

/**
 * @brief Initialize socket application receive and send (watermark) related resources.
 *
 * @param context Pointer to socket application context, contains transfer session information.
 */
static void socket_app_rx_tx_watermark_init(socket_app_context_t *context)
{
    socket_app_transfer_session *transfer_session = QOSA_NULL;

    /* Get transfer session structure pointer */
    transfer_session = &context->transfer_session;

    /* Zero the entire transfer session structure */
    qosa_memset(transfer_session, 0, sizeof(socket_app_transfer_session));

    /* Initialize send water level queue */
    qosa_buffer_queue_init(&transfer_session->send_wm_ptr, SOCKET_APP_WATERMARK_LIMIT, &transfer_session->send_q);

    /* If send water level queue is not empty, clear the queue */
    if (QOSA_FALSE == qosa_buffer_is_wm_empty(&transfer_session->send_wm_ptr))
    {
        qosa_buffer_empty_queue(&transfer_session->send_wm_ptr);
    }

    /* Set send water level queue initial parameters */
    transfer_session->send_wm_ptr.current_bytes = 0;
    transfer_session->send_wm_ptr.low_watermark = SOCKET_APP_WATERMARK_LOW_LIMIT;
    transfer_session->send_wm_ptr.high_watermark = SOCKET_APP_WATERMARK_HIGH_LIMIT;

    /* Set send water level related callback functions and context data */
    transfer_session->send_wm_ptr.wm_low_water_cb = socket_app_tx_wm_low_level_cb;
    transfer_session->send_wm_ptr.wm_low_water_argv = (void *)context;
    transfer_session->send_wm_ptr.wm_high_water_cb = socket_app_tx_wm_high_level_cb;
    transfer_session->send_wm_ptr.wm_high_water_argv = (void *)context;
    transfer_session->send_wm_ptr.wm_in_queue_cb = QOSA_NULL;
    transfer_session->send_wm_ptr.wm_become_empty_cb = QOSA_NULL;
    transfer_session->send_wm_ptr.wm_become_nonempty_cb = socket_app_tx_wm_non_empty_cb;
    transfer_session->send_wm_ptr.wm_become_nonempty_argv = (void *)context;

    /* Initialize receive water level queue */
    qosa_buffer_queue_init(&transfer_session->recv_wm_ptr, SOCKET_APP_WATERMARK_LIMIT, &transfer_session->recv_q);

    /* If receive water level queue is not empty, clear the queue */
    if (QOSA_FALSE == qosa_buffer_is_wm_empty(&transfer_session->recv_wm_ptr))
    {
        qosa_buffer_empty_queue(&transfer_session->recv_wm_ptr);
    }

    /* Set receive water level queue initial parameters */
    transfer_session->recv_wm_ptr.current_bytes = 0;
    transfer_session->recv_wm_ptr.low_watermark = SOCKET_APP_WATERMARK_LOW_LIMIT;
    transfer_session->recv_wm_ptr.high_watermark = SOCKET_APP_WATERMARK_HIGH_LIMIT;

    /* Set receive water level related callback functions and context data */
    transfer_session->recv_wm_ptr.wm_low_water_cb = socket_app_rx_wm_low_level_cb;
    transfer_session->recv_wm_ptr.wm_low_water_argv = (void *)context;
    transfer_session->recv_wm_ptr.wm_high_water_cb = socket_app_rx_wm_high_level_cb;
    transfer_session->recv_wm_ptr.wm_high_water_argv = (void *)context;
    transfer_session->recv_wm_ptr.wm_in_queue_cb = QOSA_NULL;
    transfer_session->recv_wm_ptr.wm_become_empty_cb = QOSA_NULL;
    transfer_session->recv_wm_ptr.wm_become_nonempty_cb = socket_app_rx_wm_non_empty_cb;
    transfer_session->recv_wm_ptr.wm_become_nonempty_argv = (void *)context;

    /* Mark receive and send water level system as ready */
    transfer_session->rx_tx_ready = QOSA_TRUE;
}

/**
 * @brief Watermark release resources
 *
 * @param[in] socket_app_context_t * context
 *         - Socket context
 */
static void socket_app_rx_tx_watermark_deinit(socket_app_context_t *context)
{
    socket_app_transfer_session *transfer_session = QOSA_NULL;

    transfer_session = &context->transfer_session;
    if (transfer_session->rx_tx_ready == QOSA_FALSE)
    {
        return;
    }
    if (transfer_session->send_wm_ptr.q_ptr != QOSA_NULL)
    {
        if (transfer_session->send_wm_ptr.current_bytes != 0)
        {
            qosa_buffer_empty_queue(&transfer_session->send_wm_ptr);
        }
        qosa_buffer_queue_destroy(&transfer_session->send_wm_ptr);
    }

    if (transfer_session->current_send_data_ptr != QOSA_NULL)
    {
        qosa_buffer_free_packet(&transfer_session->current_send_data_ptr);
        transfer_session->current_send_data_ptr = QOSA_NULL;
    }

    if (transfer_session->recv_wm_ptr.q_ptr != QOSA_NULL)
    {
        if (transfer_session->recv_wm_ptr.current_bytes != 0)
        {
            qosa_buffer_empty_queue(&transfer_session->recv_wm_ptr);
        }
        qosa_buffer_queue_destroy(&transfer_session->recv_wm_ptr);
    }

    if (transfer_session->current_recv_data_ptr != QOSA_NULL)
    {
        qosa_buffer_free_packet(&transfer_session->current_recv_data_ptr);
        transfer_session->current_recv_data_ptr = QOSA_NULL;
    }

    transfer_session->rx_tx_ready = QOSA_FALSE;
}
/**
 * @brief Socket data send function
 *
 * Mainly used when TCPIP protocol stack underlying layer enters unwritable state and recovers,
 * continue data sending through this function
 *
 * @param[in] socket_app_context_t * context
 *          - Socket context
 */
static void qcm_socket_app_soc_write(socket_app_context_t *context)
{
    int         pullup_len = 0;              // Get cache length in Watermark
    int         write_len = 0;               // Expected send data length
    int         send_len = 0;                // Current already sent length
    int         send_ret = 0;                // Call send function return value
    qosa_bool_t send_continue = QOSA_FALSE;  // Continue sending data
    char       *write_buff = QOSA_NULL;

    socket_app_transfer_session *transfer_session = &context->transfer_session;

    if (transfer_session->rx_tx_ready == QOSA_FALSE)
    {
        QLOGE("send_ready_false");
        return;
    }

    if (transfer_session->send_wm_high == QOSA_TRUE)
    {
        QLOGV("send_wm_high");
        return;
    }

    // Get Watermark remaining unsent space size
    write_len = MIN(
        SOCKET_APP_SINGLE_SEND_LEN,
        (qosa_buffer_queue_cnt(&transfer_session->send_wm_ptr) + qosa_buffer_length_packet(transfer_session->current_send_data_ptr))
    );
    QLOGD("write_len=%d", write_len);

    if (write_len == 0)
        return;

    write_buff = qosa_malloc(write_len);
    if (write_buff == QOSA_NULL)
        return;

    while (transfer_session->current_send_data_ptr != QOSA_NULL
           || (transfer_session->current_send_data_ptr = (qosa_buffer_block_t *)qosa_buffer_dequeue(&transfer_session->send_wm_ptr)) != QOSA_NULL)
    {
        pullup_len = qosa_buffer_pullup(&transfer_session->current_send_data_ptr, write_buff + send_len, write_len);

        write_len -= pullup_len;
        send_len += pullup_len;

        if (write_len == 0)
            break;
    }

    if (send_len <= 0)
    {
        qosa_free(write_buff);
        return;
    }

    send_ret = qcm_socket_send(context->sockfd, write_buff, send_len);
    QLOGD("send_ret=%d", send_ret);

    // If send length is not equal to input length, it may be an error, or TCPIP protocol stack underlying sendbuff buffer full
    if (send_ret != send_len)
    {
        if (send_ret < 0)
        {
            qosa_buffer_pushdown(&transfer_session->current_send_data_ptr, (void *)write_buff, send_len);
        }
        else
        {
            qosa_buffer_pushdown(&transfer_session->current_send_data_ptr, (void *)(write_buff + send_ret), send_len - send_ret);
            // Some platforms must trigger wouldblock again to really enter wait write event state
            send_continue = QOSA_TRUE;
        }
    }
    else
    {
        send_continue = QOSA_TRUE;
    }

    qosa_free(write_buff);

    // When there are more than two send release event queue caches, stop continuing to send release events to prevent msgq full
    if (send_continue == QOSA_TRUE && context->send_release_count < 3)
    {
        // Send data again
        socket_app_event_report(SOCKET_APP_NOBLOCK_MSG_SEND_IND, context);
    }
}

/**
 * @brief Socket close function
 *
 * When socket connection encounters exception, need to actively close socket connection state, although this function execution is in the same task
 * context, but directly closing socket connection here will cause socket task internal state machine difficult to manage, so
 * here notify socket task to perform socket close operation by sending message
 *
 * @param[in] socket_app_context_t * context
 *          - Socket context
 */
static void qcm_socket_app_soc_close(socket_app_context_t *context)
{
    context->connect_state = SOCKET_APP_CONNECT_STATE_CLOSING;
    socket_app_event_report(SOCKET_APP_NOBLOCK_MSG_CLOSE, context);
}

/**
 * @brief Socket data read function
 *
 * @param[in] socket_app_context_t * context
 *          - Socket context
 */
/**
 * @brief Read data from socket and store to receive queue
 *
 * This function is responsible for reading data from specified socket connection, encapsulating the read data into memory items,
 * and adding them to the transfer session's receive queue. The function checks connection status and receive watermark,
 * ensuring data read operation is only performed in appropriate state.
 *
 * @param context Socket application context pointer, contains socket descriptor, connection status and transfer session information
 * @return No return value
 */
static void qcm_socket_app_soc_read(socket_app_context_t *context)
{
    int                  ret = 0;
    char                 recv_buff[RECV_BUFF_MAX_LEN] = {0};
    qosa_buffer_block_t *read_data_item = QOSA_NULL;

    socket_app_transfer_session *transfer_session = &context->transfer_session;

    /* Check connection status, return directly if not connected state */
    if (context->connect_state != SOCKET_APP_CONNECT_STATE_CONNECT)
    {
        QLOGE("connect_state != SOCKET_APP_CONNECT_STATE_CONNECT");
        return;
    }

    /* Check receive watermark, return directly if high water level mark is true */
    if (transfer_session->recv_wm_high == QOSA_TRUE)
    {
        QLOGV("recv_wm_high");
        return;
    }

    /* Read data from socket to receive buffer */
    ret = qcm_socket_read(context->sockfd, recv_buff, RECV_BUFF_MAX_LEN);
    if (ret > 0)
    {
        /* Encapsulate read data into memory item and add to queue tail */
        qosa_buffer_pushdown_tail(&read_data_item, (void *)recv_buff, ret);
        QLOGD("ret=%d recv_buff=%s", ret, recv_buff);
    }

    /* If read function encounters error or length is 0, return directly, no further processing */

    /* If successfully read data, add data item to receive queue */
    if (read_data_item != QOSA_NULL)
    {
        qosa_buffer_enqueue(&transfer_session->recv_wm_ptr, &read_data_item);

        //TODO: If need event notification for each data read, add event notification function here
    }
}

/**
 * @brief Socket active notification event processing function
 */
static void qcm_socket_app_event_process(void *argv)
{
    socket_app_event_ind_t *ind_msg = (socket_app_event_ind_t *)argv;
    socket_app_context_t   *context = (socket_app_context_t *)ind_msg->argv;

    switch (ind_msg->event_mask)
    {
        // Handle socket connection event
        case QCM_SOCK_CONNECT_EVENT:
            QLOGV("QCM_SOCK_CONNECT_EVENT");
            if (ind_msg->result_code == QCM_SOCK_SUCCESS)
            {
                QLOGD("socketfd=%d connect success", ind_msg->sockfd);
                context->connect_state = SOCKET_APP_CONNECT_STATE_CONNECT;
            }
            else
            {
                QLOGE("socketfd=%d connect error", ind_msg->sockfd);
            }
            break;
        case QCM_SOCK_READ_EVENT:
            QLOGV("QCM_SOCK_READ_EVENT");
            qcm_socket_app_soc_read(context);
            break;
        case QCM_SOCK_WRITE_EVENT:
            QLOGV("QCM_SOCK_WRITE_EVENT");
            qcm_socket_app_soc_write(context);
            break;
        case QCM_SOCK_CLOSE_EVENT:
            QLOGV("QCM_SOCK_CLOSE_EVENT");
            qcm_socket_app_soc_close(context);
            break;
        default:
            break;
    }
    qosa_free(ind_msg);
}

/**
 * @brief Create non-blocking socket
 *
 * 1. First perform DNS resolution
 * 2. Use qcm_socket_create to create non-blocking socket
 *
 * @return int
 *          - Return corresponding socket handle
 *          - Return negative value on failure, specific information refer to qosa_sock_err_code
 */
static int qcm_socket_app_socket_create(socket_app_context_t *context)
{
    int                    sockfd = -1;
    int                    ret = 0;
    struct qosa_addrinfo_s hints, *rp, *result;
    qosa_ip_addr_t         remote_ip = {0};

    // First perform PDP activation and check
    if (qcm_socket_app_datacall_active() != 0)
    {
        QLOGE("pdp error");
        return -1;
    }

    // Perform DNS resolution
    hints.ai_family = QCM_AF_INET;
    if (qosa_dns_syn_getaddrinfo(SOCKET_BLOCK_DEMO_SIMID, SOCKET_BLOCK_DEMO_PDPID, context->hostname, &hints, &result) != QOSA_DNS_RESULT_OK)
    {
        QLOGE("dns_syn_getaddrinfo error");
        return -1;
    }
    else
    {
        QLOGV("dns_syn_getaddrinfo success");
        for (rp = result; rp != QOSA_NULL; rp = rp->ai_next)
        {
            qosa_memset(&remote_ip, 0, sizeof(qosa_ip_addr_t));
            if (hints.ai_family == QCM_AF_INET)
            {
                // Handle IPv4 address
                inet_pton(AF_INET, rp->ip_addr, &remote_ip.addr.ipv4_addr);
                remote_ip.ip_vsn = QOSA_PDP_IPV4;
            }

            // Use qcm_socket_create to create non-blocking socket
            sockfd = qcm_socket_create(0, 1, rp->ai_family, QCM_SOCK_STREAM, QCM_TCP_PROTOCOL, 0, QOSA_FALSE);
            if (sockfd < 0)
            {
                QLOGE("qcm_socket_create error");
                qosa_dns_result_free(result);
                return -1;
            }

            // Register event callback function
            ret = qcm_socket_register_event(
                sockfd,
                QCM_SOCK_WRITE_EVENT | QCM_SOCK_READ_EVENT | QCM_SOCK_CLOSE_EVENT | QCM_SOCK_CONNECT_EVENT,
                socket_app_register_event_cb,
                context
            );

            QLOGD("ip--->%s port=%d", rp->ip_addr, context->hostport);
            // Use non-blocking method to establish TCP connection
            ret = qcm_socket_connect(sockfd, &remote_ip, context->hostport);
            if (ret == 0 || ret == QCM_SOCK_WODBLOCK)
            {
                QLOGV("qcm_socket_connect continue");
                break;
            }
            else
            {
                qcm_socket_close(sockfd);
                sockfd = -1;
            }
        }
        // Free DNS resources
        qosa_dns_result_free(result);
    }
    QLOGD("sockfd=%d", sockfd);
    return sockfd;
}

/**
 * @brief Handle socket active data sending
 */
static void qcm_socket_app_send_process(void *argv)
{
    socket_app_context_t *context = (socket_app_context_t *)argv;
    context->send_release_count--;
    qcm_socket_app_soc_write(context);
}

/**
 * @brief Handle socket active close
 */
static void qcm_socket_app_close_process(void *argv)
{
    QOSA_UNUSED(argv);
    socket_app_context_t *context = (socket_app_context_t *)argv;
    qcm_socket_linger_t   q_linger;

    if (context->connect_state == SOCKET_APP_CONNECT_STATE_IDLE)
    {
        return;
    }

    // Set socket linger time to 0, close socket immediately
    q_linger.on_off = 1;
    q_linger.linger_val = 0;
    qcm_socket_set_opt(context->sockfd, QCM_SOCK_LINGER_OPT, &q_linger);

    qcm_socket_close(context->sockfd);
    context->sockfd = -1;
    context->connect_state = SOCKET_APP_CONNECT_STATE_CLOSED;
}

/**
 * @brief Non-blocking socket thread
 */
static void qcm_socket_app_demo_thread(void *argv)
{
    int                   sockfd = -1;
    socket_app_msg_info_t msg_info;
    int                   ret = 0;
    socket_app_context_t *context = &g_socket_context;

    QOSA_UNUSED(argv);
    // Wait for a period of time before starting demo, convenient for network registration and log capture
    qosa_task_sleep_sec(10);

    context->connect_state = SOCKET_APP_CONNECT_STATE_INIT;
    qosa_memset(context, 0, sizeof(socket_app_context_t));
    qosa_strcpy(context->hostname, SOCKET_NOBLOCK_CONNECT_SERVER_NAME);
    context->hostport = SOCKET_NOBLOCK_CONNECT_SERVER_PORT;

    // Create non-blocking socket
    sockfd = qcm_socket_app_socket_create(context);
    if (sockfd < 0)
    {
        QLOGE("qcm_socket_connect error");
        return;
    }

    context->connect_state = SOCKET_APP_CONNECT_STATE_CONNECTING;
    context->sockfd = sockfd;
    socket_app_rx_tx_watermark_init(context);
    while (1)
    {
        qosa_memset(&msg_info, 0, sizeof(socket_app_msg_info_t));
        ret = qosa_msgq_wait(g_socket_msgq, (qosa_uint8_t *)&msg_info, sizeof(socket_app_msg_info_t), QOSA_WAIT_FOREVER);
        if (ret != QOSA_OK)
        {
            QLOGE("qosa_msgq_wait error");
            continue;
        }

        switch (msg_info.event_type)
        {
            case SOCKET_APP_NOBLOCK_MSG_EVENT_IND:
                // Socket event notification
                qcm_socket_app_event_process(msg_info.argv);
                break;
            case SOCKET_APP_NOBLOCK_MSG_SEND_IND:
                // Handle user active data sending
                qcm_socket_app_send_process(msg_info.argv);
                break;
            case SOCKET_APP_NOBLOCK_MSG_CLOSE:
                // Handle user active socket close
                qcm_socket_app_close_process(msg_info.argv);
                goto exit;
            default:
                break;
        }
    }
exit:
    socket_app_rx_tx_watermark_deinit(context);
}

/**
 * @brief Socket active data sending
 * Send data to be sent into watermark, after completion watermark non empty will send event notification to socket task for sending
 */
void qcm_socket_app_send_data(char *data, int data_len)
{
    socket_app_context_t        *context = &g_socket_context;
    socket_app_transfer_session *transfer_session = &context->transfer_session;
    qosa_buffer_block_t         *item_ptr = QOSA_NULL;

    item_ptr = qosa_buffer_new_block();
    if (item_ptr == QOSA_NULL)
    {
        QLOGE("qosa_buffer_new_block error");
        return;
    }

    qosa_buffer_pushdown_tail(&item_ptr, data, data_len);
    qosa_buffer_enqueue(&transfer_session->send_wm_ptr, &item_ptr);
}

/**
 * @brief Socket active close
 */
void qcm_socket_app_close(void)
{
    socket_app_event_report(SOCKET_APP_NOBLOCK_MSG_CLOSE, QOSA_NULL);
}

/**
 * @brief Verify socket connection using non-blocking socket
 */
void unir_qcm_socket_noblock_demo_init(void)
{
    int         err = 0;
    qosa_task_t sock_task = QOSA_NULL;

    err = qosa_msgq_create(&g_socket_msgq, sizeof(socket_app_msg_info_t), 20);
    if (err != QOSA_OK)
    {
        QLOGE("qosa_msgq_create error");
        return;
    }

    err = qosa_task_create(
        &sock_task,
        SOCKET_NOBLOCK_DEMO_TASK_STACK_SIZE,
        SOCKET_NOBLOCK_DEMO_TASK_PRIO,
        "socket_app_task",
        qcm_socket_app_demo_thread,
        QOSA_NULL
    );
    if (err != QOSA_OK)
    {
        qosa_msgq_delete(g_socket_msgq);
        QLOGE("task create error");
        return;
    }
}
