/*****************************************************************/ /**
* @file qcm_socket_server_nonblock_demo.c
* @brief Non-blocking mode TCP server demo using qcm_socket_adp.h API
* @author larson.li@quectel.com
* @date 2026-04-09
*
* @copyright Copyright (c) 2023 Quectel Wireless Solution, Co., Ltd.
* All Rights Reserved. Quectel Wireless Solution Proprietary and Confidential.
*
* @par EDIT HISTORY FOR MODULE
* <table>
* <tr><th>Date <th>Version <th>Author <th>Description
* <tr><td>2026-04-09 <td>1.0 <td>larson.li <td> Init - Non-blocking TCP Server Demo
* </table>
**********************************************************************/

#include "qosa_def.h"
#include "qosa_sys.h"
#include "qosa_log.h"
#include "qcm_socket_adp.h"
#include "qosa_datacall.h"

#define QOS_LOG_TAG                       LOG_TAG

/* ==================== Server Configuration ==================== */
#define SOCKET_SERVER_DEMO_TASK_STACK_SIZE       (20 * 1024)
#define SOCKET_SERVER_DEMO_TASK_PRIO             QOSA_PRIORITY_NORMAL

#define SOCKET_SERVER_LISTEN_PORT                12345      // Server listening port
#define SOCKET_SERVER_BACKLOG                    5         // Max pending connections
#define SOCKET_SERVER_MAX_CLIENTS                10        // Max concurrent clients
#define SOCKET_SERVER_BUFF_MAX_LEN               1024      // Buffer size

#define SOCKET_SERVER_SIMID                      0         // SIM card ID
#define SOCKET_SERVER_PDPID                      1         // PDP context ID
#define SOCKET_SERVER_ACTIVE_TIMEOUT             30        // PDP activation timeout (seconds)

/**
 * @brief Socket application message type enumeration definition
 *
 * This enumeration defines non-blocking message types used in socket applications,
 * used to identify different socket events and operation indications.
 */
typedef enum
{
    SOCKET_APP_NOBLOCK_MSG_EVENT_IND = 1, /*!< Socket event notification */
    SOCKET_APP_CLIENT_MSG_EVENT_IND = 2, /*!< Client event notification */
} socket_app_msg_type_e;

/* ==================== Data Structure ==================== */

/**
 * @brief Socket application message information structure
 */
typedef struct
{
    socket_app_msg_type_e event_type; /*!< Message event type */
    void                 *argv;       /*!< Message parameter pointer, points to specific parameter data */
} socket_app_msg_info_t;

/**
 * @brief Client connection management structure
 */
typedef struct {
    int          socket_fd;                  // Client socket handle
    qosa_ip_addr_t remote_ip;                // Client IP address
    int          remote_port;                // Client port
    qosa_ip_addr_t local_ip;                 // Server local IP
    int          local_port;                 // Server local port
    qosa_bool_t  active;                     // Connection active flag
    qosa_uint32_t recv_count;                // Data received count
    qosa_uint32_t send_count;                // Data sent count
    char        *send_buffer;                // Pending send buffer
    int          send_buffer_len;            // Pending send buffer length
    int          send_offset;                // Current send offset
} socket_client_info_t;

/**
 * @brief Server context structure
 */
typedef struct {
    int                listen_socket;        // Server listening socket
    socket_client_info_t clients[SOCKET_SERVER_MAX_CLIENTS];  // Client array
    int                client_count;         // Current client count
    qosa_bool_t        server_running;       // Server running flag
} socket_server_context_t;

/**
 * @brief Socket application event indication structure
 *
 * This structure is used to encapsulate socket-related event information, including socket file descriptor,
 * event mask, result code, and user-defined parameters
 */
typedef struct
{
    int   sockfd;      /*!< socket handle */
    int   event_mask;  /*!< Event mask, identifies the type of event that occurred */
    int   result_code; /*!< Operation result code, indicates the execution result of the operation */
    void *argv;        /*!< User-defined parameter pointer, used to pass additional data */
} socket_app_event_ind_t;

/* ==================== Global Variables ==================== */
static socket_server_context_t g_server_ctx = {
    .listen_socket = -1,
    .client_count = 0,
    .server_running = QOSA_FALSE
};

static qosa_msgq_t g_socket_msgq;

/* ==================== Helper Functions ==================== */
static void qcm_socket_server_on_client_event(int sock_hndl, int event, int code, void *user_argv);

/**
 * @brief Check and activate PDP data connection
 *
 * @return 0 on success, -1 on failure
 */
static int qcm_socket_server_datacall_active(void)
{
    qosa_datacall_conn_t    conn = 0;
    qosa_datacall_ip_info_t info = {0};
    qosa_datacall_errno_e   ret = 0;
    qosa_pdp_context_t      pdp_ctx = {0};
    char                    ip4addr_buf[CONFIG_QOSA_INET_ADDRSTRLEN] = {0};
    char                    ip6addr_buf[CONFIG_QOSA_INET6_ADDRSTRLEN] = {0};

    // Configure PDP context: APN, IP type
    // If the operator has restrictions on the APN during registration, needs to be set the APN provided by the operator
    const char *apn_str = "CTNET";
    pdp_ctx.apn_valid = QOSA_TRUE;
    pdp_ctx.pdp_type = QOSA_PDP_TYPE_IPV6;  //ipv6
    if (pdp_ctx.apn_valid)
    {
        qosa_memcpy(pdp_ctx.apn, apn_str, qosa_strlen(apn_str));
    }

    ret = qosa_datacall_set_pdp_context(SOCKET_SERVER_SIMID, SOCKET_SERVER_PDPID, &pdp_ctx);
    QLOGI("set pdp context, ret=%d", ret);

    conn = qosa_datacall_conn_new(SOCKET_SERVER_SIMID, SOCKET_SERVER_PDPID, QOSA_DATACALL_CONN_TCPIP);

    qosa_memset(ip4addr_buf, 0, sizeof(ip4addr_buf));
    qosa_memset(ip6addr_buf, 0, sizeof(ip6addr_buf));

    if (QOSA_DATACALL_ERR_NO_ACTIVE == qosa_datacall_get_ip_info(conn, &info))
    {
        QLOGD("PDP not active, activating... sim_id=%d, pdp_id=%d", SOCKET_SERVER_SIMID, SOCKET_SERVER_PDPID);

        ret = qosa_datacall_start(conn, SOCKET_SERVER_ACTIVE_TIMEOUT);
        if (QOSA_DATACALL_OK != ret)
        {
            QLOGE("PDP activation failed: ret=%x", ret);
            return -1;
        }
        QLOGD("PDP activated successfully");
            // Get IP info from datacall
        ret = qosa_datacall_get_ip_info(conn, &info);
        QLOGI("pdpid=%d,simid=%d", info.simcid.pdpid, info.simcid.simid);
        QLOGI("ip_type=%d", info.ip_type);

        if (info.ip_type == QOSA_PDP_IPV4)
        {
            // IPv4 info            
            qosa_ip_addr_inet_ntop(QOSA_IP_ADDR_AF_INET, &info.ipv4_ip.addr.ipv4_addr, ip4addr_buf, sizeof(ip4addr_buf));
            QLOGI("ipv4 addr:%s", ip4addr_buf);
        }
        else if (info.ip_type == QOSA_PDP_IPV6)
        {
            // IPv6 info
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
    else
    {
        QLOGD("PDP already active, IP type: %d", info.ip_type);
    }
    
    return 0;
}

/**
 * @brief Find an available client slot
 *
 * @return Index of available slot, or -1 if full
 */
static int qcm_socket_server_find_available_slot(void)
{
    int i;
    for (i = 0; i < SOCKET_SERVER_MAX_CLIENTS; i++)
    {
        if (g_server_ctx.clients[i].active == QOSA_FALSE)
        {
            return i;
        }
    }
    QLOGE("Client slots full, max %d clients", SOCKET_SERVER_MAX_CLIENTS);
    return -1;
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
 * @param sockfd socket handle
 * @param event_mask Event mask, identifies the type of event that occurred
 * @param result Operation result code
 * @param argv Additional parameter pointer
 */
static void socket_app_register_event_cb(int socket_hd, int event_mask, int result, void *argv)
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
    ind_msg->sockfd = socket_hd;
    ind_msg->event_mask = event_mask;
    ind_msg->result_code = result;
    ind_msg->argv = argv;

    /* Report socket application event */
    if(socket_hd == g_server_ctx.listen_socket)
    {
        socket_app_event_report(SOCKET_APP_NOBLOCK_MSG_EVENT_IND, ind_msg);
    }
    else
    {
        socket_app_event_report(SOCKET_APP_CLIENT_MSG_EVENT_IND, ind_msg);
    }
}

/**
 * @brief Accept new client connection callback (called when ACCEPT_EVENT fires)
 */
static void qcm_socket_server_on_accept(int sock_hndl, int code, void *user_argv)
{
    int client_idx = -1;
    int new_client_socket = -1;
    qosa_ip_addr_t remote_addr = {0};
    qosa_ip_addr_t local_addr = {0};
    int remote_port = 0;
    int local_port = 0;
    char ip_str[32] = {0};

    QLOGV("Accept event on socket %d, code=%d", sock_hndl, code);

    if(code != QOSA_OK)
    {
        QLOGE("Accept failed: code=%d", code);
        return;
    }
    /* Find available client slot */
    client_idx = qcm_socket_server_find_available_slot();
    if (client_idx < 0)
    {
        QLOGE("No available client slot");
        return;
    }

    /* Accept new client connection */
    new_client_socket = qcm_socket_accept(
        g_server_ctx.listen_socket,
        &remote_addr,
        &remote_port,
        &local_addr,
        &local_port
    );

    if (new_client_socket < 0)
    {
        QLOGE("Accept failed: ret=%d", new_client_socket);
        return;
    }

    /* Register client in context */
    g_server_ctx.clients[client_idx].socket_fd = new_client_socket;
    g_server_ctx.clients[client_idx].remote_ip = remote_addr;
    g_server_ctx.clients[client_idx].remote_port = remote_port;
    g_server_ctx.clients[client_idx].local_ip = local_addr;
    g_server_ctx.clients[client_idx].local_port = local_port;
    g_server_ctx.clients[client_idx].active = QOSA_TRUE;
    g_server_ctx.clients[client_idx].recv_count = 0;
    g_server_ctx.clients[client_idx].send_count = 0;
    g_server_ctx.clients[client_idx].send_buffer = NULL;
    g_server_ctx.clients[client_idx].send_buffer_len = 0;
    g_server_ctx.clients[client_idx].send_offset = 0;
    g_server_ctx.client_count++;

    /* Register event callbacks for new client */
    qcm_socket_register_event(
        new_client_socket,
        QCM_SOCK_READ_EVENT | QCM_SOCK_WRITE_EVENT | QCM_SOCK_CLOSE_EVENT,
        socket_app_register_event_cb,
        (void *)(qosa_ptr)client_idx
    );

    /* Log client connection */
    if (remote_addr.ip_vsn == QOSA_PDP_IPV4)
    {
        qcm_inet_ntop(AF_INET, &remote_addr.addr.ipv4_addr, ip_str, sizeof(ip_str));
        QLOGD("New client accepted: [%s]:%d (socket=%d, slot=%d)", ip_str, remote_port, new_client_socket, client_idx);
    }
    else
    {
        qcm_inet_ntop(AF_INET6, &remote_addr.addr.ipv6_addr, ip_str, sizeof(ip_str));
        QLOGD("New client accepted: [%s]:%d (socket=%d, slot=%d)", ip_str, remote_port, new_client_socket, client_idx);
    }
}

/**
 * @brief Client event callback (READ/WRITE/CLOSE events)
 */
static void qcm_socket_server_on_client_event(int sock_hndl, int event, int code, void *user_argv)
{
    int client_idx = (int)(qosa_ptr)user_argv;
    socket_client_info_t *client = &g_server_ctx.clients[client_idx];
    int ret;
    int send_len = 0;
    char buffer[SOCKET_SERVER_BUFF_MAX_LEN];

    QLOGV("Client[%d] event=0x%x code=%d", client_idx, event, code);

    if (event == QCM_SOCK_READ_EVENT)
    {
        /* ===== Handle Read Event ===== */
        QLOGV("Client[%d] READ_EVENT", client_idx);
        
        qosa_memset(buffer, 0, sizeof(buffer));
        ret = qcm_socket_read(sock_hndl, buffer, SOCKET_SERVER_BUFF_MAX_LEN - 1);

        if (ret > 0)
        {
            /* Data received successfully */
            client->recv_count++;
            buffer[ret] = '\0';
            QLOGD("Received from client[%d]: %d bytes, data=%s", client_idx, ret, buffer);

            /* Prepare response with echo */
            if (client->send_buffer != NULL)
            {
                qosa_free(client->send_buffer);
            }
            client->send_buffer_len = qosa_snprintf(
                NULL, 0,
                "[Server Echo] Your message: %s",
                buffer
            ) + 1;

            client->send_buffer = (char *)qosa_malloc(client->send_buffer_len);
            if (client->send_buffer == NULL)
            {
                QLOGE("Memory allocation failed for client[%d]", client_idx);
                client->active = QOSA_FALSE;
                qcm_socket_close(sock_hndl);
                return;
            }

            qosa_snprintf(
                client->send_buffer,
                client->send_buffer_len,
                "[Server Echo] Your message: %s",
                buffer
            );
            client->send_buffer_len--;  // Exclude null terminator
            client->send_offset = 0;
            send_len = qcm_socket_send(sock_hndl, client->send_buffer, client->send_buffer_len);
            if(send_len == QCM_SOCK_WODBLOCK)
            {
                QLOGD("Initial send would block for client[%d], will retry on WRITE event", client_idx);
            }
            else if(send_len > 0)
            {
                if(send_len == client->send_buffer_len)
                {
                    client->send_buffer_len -= send_len;
                    client->send_offset = 0;
                }
                else
                {
                    client->send_buffer_len -= send_len;
                    client->send_offset += send_len;
                }
            }
            else
            {
                QLOGE("Send error to client[%d]: ret=%d", client_idx, send_len);
            }

            /* Trigger WRITE event to send response */
            QLOGV("Queuing response for client[%d], will send on WRITE event", client_idx);
        }
        else if (ret == QCM_SOCK_BROKEN)
        {
            /* Client closed connection */
            QLOGD("Client[%d] closed connection (received %u, sent %u)",
                  client_idx, client->recv_count, client->send_count);
            client->active = QOSA_FALSE;
            if (client->send_buffer != NULL)
            {
                qosa_free(client->send_buffer);
                client->send_buffer = NULL;
            }
            qcm_socket_close(sock_hndl);
            g_server_ctx.client_count--;
        }
        else if (ret == QCM_SOCK_WODBLOCK)
        {
            QLOGV("Client[%d] no data available", client_idx);
        }
        else
        {
            /* Error occurred */
            QLOGE("Receive error from client[%d]: ret=%d", client_idx, ret);
            client->active = QOSA_FALSE;
            if (client->send_buffer != NULL)
            {
                qosa_free(client->send_buffer);
                client->send_buffer = NULL;
            }
            qcm_socket_close(sock_hndl);
            g_server_ctx.client_count--;
        }
    }
    else if (event == QCM_SOCK_WRITE_EVENT)
    {
        /* ===== Handle Write Event ===== */
        QLOGV("Client[%d] WRITE_EVENT", client_idx);

        if (client->send_buffer != NULL && client->send_buffer_len > 0)
        {
            /* Send pending data */
            ret = qcm_socket_send(
                sock_hndl,
                client->send_buffer + client->send_offset,
                client->send_buffer_len - client->send_offset
            );

            if (ret > 0)
            {
                client->send_offset += ret;
                client->send_count++;
                QLOGD("Sent to client[%d]: %d bytes (total %d/%d)", 
                      client_idx, ret, client->send_offset, client->send_buffer_len);

                if (client->send_offset >= client->send_buffer_len)
                {
                    /* All data sent */
                    QLOGD("Client[%d] send complete", client_idx);
                    qosa_free(client->send_buffer);
                    client->send_buffer = NULL;
                    client->send_buffer_len = 0;
                    client->send_offset = 0;
                }
            }
            else if (ret == QCM_SOCK_WODBLOCK)
            {
                QLOGV("Client[%d] send would block, will retry on next WRITE event", client_idx);
            }
            else
            {
                QLOGE("Send error to client[%d]: ret=%d", client_idx, ret);
                client->active = QOSA_FALSE;
                if (client->send_buffer != NULL)
                {
                    qosa_free(client->send_buffer);
                    client->send_buffer = NULL;
                }
                qcm_socket_close(sock_hndl);
                g_server_ctx.client_count--;
            }
        }
    }
    else if (event == QCM_SOCK_CLOSE_EVENT)
    {
        /* ===== Handle Close Event ===== */
        QLOGD("Client[%d] CLOSE_EVENT, code=%d", client_idx, code);
        client->active = QOSA_FALSE;
        if (client->send_buffer != NULL)
        {
            qosa_free(client->send_buffer);
            client->send_buffer = NULL;
        }
        qcm_socket_close(sock_hndl);
        g_server_ctx.client_count--;
    }
}

/**
 * @brief Socket active notification event processing function
 */
static void qcm_socket_app_event_process(void *argv)
{
    socket_app_event_ind_t *ind_msg = (socket_app_event_ind_t *)argv;

    switch (ind_msg->event_mask)
    {
        case QCM_SOCK_ACCEPT_EVENT:
            qcm_socket_server_on_accept(ind_msg->sockfd,ind_msg->result_code,ind_msg->argv);
            break;

        default:
            break;
    }
}

/* ==================== Main Server Function ==================== */

/**
 * @brief Non-blocking TCP server main processing function
 *
 * This function implements an event-driven non-blocking TCP server that:
 * 1. Activates PDP data connection
 * 2. Creates a non-blocking listening socket
 * 3. Registers event callbacks for ACCEPT events
 * 4. Processes all client events through callbacks
 * 5. Handles up to 10 concurrent clients
 *
 * @param argv Unused parameter
 */
static void qcm_socket_server_main(void *argv)
{
    QOSA_UNUSED(argv);
    
    int ret = 0;
    socket_app_event_ind_t *ind_msg;
    socket_app_msg_info_t msg_info;

    QLOGD("========== Non-Blocking TCP Server Demo Started ==========");

    /* Step 1: Activate PDP data connection */
    QLOGD("Step 1: Activating PDP...");
    if (qcm_socket_server_datacall_active() != 0)
    {
        QLOGE("Failed to activate PDP, exiting");
        return;
    }

    /* Step 2: Create non-blocking listening socket */
    QLOGD("Step 2: Creating non-blocking listening socket on port %d...", SOCKET_SERVER_LISTEN_PORT);
    g_server_ctx.listen_socket = qcm_socket_create(
        SOCKET_SERVER_SIMID,
        SOCKET_SERVER_PDPID,
        QCM_AF_INET6,                    // Use IPv4
        QCM_SOCK_STREAM,                // TCP stream socket
        QCM_TCP_PROTOCOL,               // TCP protocol
        SOCKET_SERVER_LISTEN_PORT,      // Bind to port 8080
        QOSA_FALSE                      // ⚠️ Non-blocking mode
    );

    if (g_server_ctx.listen_socket < 0)
    {
        QLOGE("Failed to create listening socket: ret=%d", g_server_ctx.listen_socket);
        return;
    }

    QLOGD("Listening socket created successfully (handle=%d)", g_server_ctx.listen_socket);

    /* Step 4: Register event callback for listening socket */
    QLOGD("Step 4: Registering ACCEPT event callback...");
    ret = qcm_socket_register_event(
        g_server_ctx.listen_socket,
        QCM_SOCK_ACCEPT_EVENT,          // Listen for new connections
        socket_app_register_event_cb,    // Callback function
        NULL                             // No user argument
    );

    /* Step 3: Start listening for connections */
    QLOGD("Step 3: Starting to listen for connections (backlog=%d)...", SOCKET_SERVER_BACKLOG);
    ret = qcm_socket_listen(g_server_ctx.listen_socket, SOCKET_SERVER_BACKLOG);
    if (ret < 0)
    {
        QLOGE("Listen failed: ret=%d", ret);
        qcm_socket_close(g_server_ctx.listen_socket);
        return;
    }

    QLOGD("Server listening on port %d. Waiting for clients...", SOCKET_SERVER_LISTEN_PORT);

    if (ret < 0)
    {
        QLOGE("Failed to register event callback: ret=%d", ret);
        qcm_socket_close(g_server_ctx.listen_socket);
        return;
    }

    /* Initialize client array */
    int i;
    for (i = 0; i < SOCKET_SERVER_MAX_CLIENTS; i++)
    {
        g_server_ctx.clients[i].active = QOSA_FALSE;
        g_server_ctx.clients[i].socket_fd = -1;
        g_server_ctx.clients[i].send_buffer = NULL;
    }

    g_server_ctx.server_running = QOSA_TRUE;

    /* Step 5: Main event loop (non-blocking) */
    QLOGD("Step 5: Entering main event loop...");
    QLOGD("In non-blocking mode, all socket operations are event-driven.");
    QLOGD("The server will simply loop and print statistics periodically.");
    
    while (g_server_ctx.server_running == QOSA_TRUE)
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
            qcm_socket_app_event_process(msg_info.argv);
            break;    
        case SOCKET_APP_CLIENT_MSG_EVENT_IND:
            ind_msg = (socket_app_event_ind_t *)msg_info.argv;
            qcm_socket_server_on_client_event(ind_msg->sockfd, ind_msg->event_mask, ind_msg->result_code, ind_msg->argv);
            break;
        default:
            break;
        }

        if(msg_info.argv)
        {
            free(msg_info.argv);
        }
    }

    /* Cleanup: Close all client connections */
    QLOGD("Closing all client connections...");
    for (i = 0; i < SOCKET_SERVER_MAX_CLIENTS; i++)
    {
        if (g_server_ctx.clients[i].active == QOSA_TRUE)
        {
            QLOGD("Closing client[%d]", i);
            if (g_server_ctx.clients[i].send_buffer != NULL)
            {
                qosa_free(g_server_ctx.clients[i].send_buffer);
                g_server_ctx.clients[i].send_buffer = NULL;
            }
            qcm_socket_close(g_server_ctx.clients[i].socket_fd);
            g_server_ctx.clients[i].active = QOSA_FALSE;
        }
    }

    /* Close listening socket */
    QLOGD("Closing listening socket...");
    qcm_socket_close(g_server_ctx.listen_socket);
    g_server_ctx.listen_socket = -1;

    QLOGD("========== Non-Blocking TCP Server Demo Ended ==========");
}

/* ==================== Initialization ==================== */

/**
 * @brief Initialize non-blocking TCP server demo
 *
 * This function creates a task to run the non-blocking TCP server.
 */
void unir_qcm_socket_server_nonblock_demo_init(void)
{
    int err = 0;
    qosa_task_t server_task = QOSA_NULL;

    QLOGD("Initializing non-blocking TCP server demo...");

    err = qosa_msgq_create(&g_socket_msgq, sizeof(socket_app_msg_info_t), 20);
    if (err != QOSA_OK)
    {
        QLOGE("qosa_msgq_create error");
        return;
    }

    err = qosa_task_create(
        &server_task,
        SOCKET_SERVER_DEMO_TASK_STACK_SIZE,
        SOCKET_SERVER_DEMO_TASK_PRIO,
        "tcp_server_nb",
        qcm_socket_server_main,
        QOSA_NULL
    );

    if (err != QOSA_OK)
    {
        QLOGE("Failed to create server task: err=%d", err);
        qosa_msgq_delete(g_socket_msgq);
        return;
    }

    QLOGD("Server task created successfully");
}
