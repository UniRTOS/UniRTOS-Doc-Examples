#include "qosa_sys.h"
#include "qosa_def.h"
#include "qosa_log.h"
#include "qcm_mqtt.h"
#include "qcm_mqtt_config.h"
#include "qcm_proj_config.h"
#include "qosa_urc.h"

#ifdef CONFIG_QCM_VTLS_FUNC
#include "qcm_vtls_cfg.h"
#endif /* CONFIG_QCM_VTLS_FUNC */

#define QOS_LOG_TAG               LOG_TAG

#define MQTT_DEMO_TASK_STACK_SIZE 4096 /*!< MQTT demo task stack size in bytes */

#define MQTT_DEMO_MSG_MAX_CNT     5

#define MQTT_DEMO_CLIENT_ID       0  /*!< MQTT client ID */
#define MQTT_DEMO_SIMID           0  /*!< SIM ID */
#define MQTT_DEMO_PDPID           1  /*!< PDP context ID for network connection */
#define MQTT_DEMO_KEEP_ALIVE      60 /*!< Keep alive time in seconds */
#define MQTT_DEMO_DELIVERY_TIME   5  /*!< Retransmission interval in seconds */
#define MQTT_DEMO_DELIVERY_CNT    3  /*!< Retransmission count */

#define MQTT_DEMO_LEN_MAX         257

#define MQTT_DEMO_SERVER_ADDR     "broker.emqx.io"              /*!< MQTT server address */
#define MQTT_DEMO_SERVER_PORT     1883                          /*!< MQTT server port */
#define MQTTS_DEMO_SERVER_PORT    8883                          /*!< MQTTS server port */

#define MQTT_DEMO_CLIENTID        "mqttx_12345678"              /*!< MQTT client unique identifier */
#define MQTT_DEMO_USERNAME        ""                            /*!< MQTT connection username, empty string means no authentication */
#define MQTT_DEMO_PASSWARD        ""                            /*!< MQTT connection password, empty string means no authentication */
#define MQTT_DEMO_TOPIC1          "mqtt_test1"                  /*!< MQTT test topic 1 for publishing and subscribing messages */
#define MQTT_DEMO_TOPIC2          "mqtt_test2"                  /*!< MQTT test topic 2 for publishing and subscribing messages */
#define MQTT_DEMO_TOPIC3          "mqtt_test_close"             /*!< MQTT close control topic for receiving shutdown commands */

#define MQTT_DEMO_TEST_CONTENT    "Hello Welcome to test mqtt!" /*!< Test message content */

#define MQTT_DEMO_TEST_MQTTS      1                             /*!< Test MQTT or MQTTS */
#define MQTT_DEMO_TEST_URC_ENABLE 1                             /*!< Enable URC reporting for test results */

/**
 * @brief mqtt_demo_msg_t structure definition
 *
 * @param msg_id Event ID for identifying different event types
 * @param param1 Data parameter 1 for passing integer data
 * @param argv Data parameter 2 for passing pointer type data
 */
typedef struct
{
    int   msg_id; /*!< Event ID */
    int   param1; /*!< Data */
    void *argv;   /*!< Data */
} mqtt_demo_msg_t;

/**
 * @brief MQTT demo context structure
 */
typedef struct
{
    qosa_uint8_t      client_id;  /*!< MQTT client handle */
    qosa_uint32_t     port;       /*!< MQTT server port */
    int               msg_id;     /*!< Packet identifier */
    qosa_msgq_t       msgq;       /*!< Message queue */
    qcm_ssl_config_t *ssl_config; /*!< SSL configuration */
} mqtt_demo_ctx_t;

// Client context
static mqtt_demo_ctx_t *g_mqtt_ctx = QOSA_NULL;

/*===========================================================================
 *  Functions
 ===========================================================================*/
/**
 * @brief Free MQTT demo context resources
 */
static void unir_mqtt_demo_ctx_free(void)
{
    /* Free message queue resources */
    if (g_mqtt_ctx->msgq != QOSA_NULL)
    {
        qosa_msgq_delete(g_mqtt_ctx->msgq);
    }

#ifdef CONFIG_QCM_VTLS_FUNC
    /* Free SSL configuration resources */
    if (g_mqtt_ctx->ssl_config != QOSA_NULL)
    {
        qosa_free(g_mqtt_ctx->ssl_config);
    }
#endif /* CONFIG_QCM_VTLS_FUNC */

    /* Free MQTT context structure */
    qosa_free(g_mqtt_ctx);
}

/**
 * @brief Initialize MQTT demo context and configuration parameters
 *
 */
static int unir_mqtt_demo_ctx_init(qcm_mqtt_config_t *mqtt_option)
{
    int ret = 0;

    if (mqtt_option == QOSA_NULL)
    {
        return -1;
    }
    // If global context is not allocated, allocate and zero memory
    if (g_mqtt_ctx == QOSA_NULL)
    {
        g_mqtt_ctx = qosa_malloc(sizeof(mqtt_demo_ctx_t));
        if (g_mqtt_ctx == QOSA_NULL)
        {
            return -1;
        }
    }
    qosa_memset(g_mqtt_ctx, 0, sizeof(mqtt_demo_ctx_t));

    // If message queue is not created, create one for MQTT message processing
    if (g_mqtt_ctx->msgq == QOSA_NULL)
    {
        ret = qosa_msgq_create(&g_mqtt_ctx->msgq, sizeof(mqtt_demo_msg_t), MQTT_DEMO_MSG_MAX_CNT);
        if (ret != QOSA_OK)
        {
            return -1;
        }
    }

    // Initialize basic fields in context
    g_mqtt_ctx->msg_id = 1;  //This parameter value can be 0 only when <QoS>=0
    g_mqtt_ctx->port = MQTT_DEMO_SERVER_PORT;

    // Fill MQTT connection configuration parameters
    mqtt_option->version = QCM_MQTT_VERSION_V3_1_1;
    mqtt_option->sim_id = MQTT_DEMO_SIMID;
    mqtt_option->pdp_cid = MQTT_DEMO_PDPID;
    mqtt_option->kalive_time = MQTT_DEMO_KEEP_ALIVE;
    mqtt_option->delivery_time = MQTT_DEMO_DELIVERY_TIME;
    mqtt_option->delivery_cnt = MQTT_DEMO_DELIVERY_CNT;
    mqtt_option->clean_session = QOSA_TRUE;
    // Other options can be configured by user, such as will message, etc.

#ifdef CONFIG_QCM_VTLS_FUNC
    // If MQTTS (SSL/TLS) is enabled, configure SSL related parameters
    if (MQTT_DEMO_TEST_MQTTS)
    {
        mqtt_option->ssl_enable = QOSA_TRUE;

        // If SSL configuration is not allocated yet, allocate memory
        if (g_mqtt_ctx->ssl_config == QOSA_NULL)
        {
            g_mqtt_ctx->ssl_config = qosa_malloc(sizeof(qcm_ssl_config_t));
            if (g_mqtt_ctx->ssl_config == QOSA_NULL)
            {
                return -1;
            }
        }

        // Zero SSL configuration structure and set default values
        qosa_memset(g_mqtt_ctx->ssl_config, 0x00, sizeof(qcm_ssl_config_t));
        g_mqtt_ctx->ssl_config->ssl_version = QCM_SSL_VERSION_3;   // Use TLS 1.2 version
        g_mqtt_ctx->ssl_config->auth_mode = QCM_SSL_VERIFY_NULL;   // Disable server certificate verification
        g_mqtt_ctx->ssl_config->transport = QCM_SSL_TLS_PROTOCOL;  // Use TLS protocol for transport
        g_mqtt_ctx->ssl_config->ssl_negotiate_timeout = 30;        // TLS handshake timeout is 30 seconds
        g_mqtt_ctx->ssl_config->ssl_log_debug = 4;                 // Enable TLS debug log output
        g_mqtt_ctx->ssl_config->sni_enable = QOSA_TRUE;            // Enable SNI (Server Name Indication)
        g_mqtt_ctx->port = MQTTS_DEMO_SERVER_PORT;                 // Update port to MQTTS port

        // Bind SSL configuration to MQTT configuration
        mqtt_option->ssl_config = g_mqtt_ctx->ssl_config;
    }
#endif /* CONFIG_QCM_VTLS_FUNC */

    return 0;
}

/**
 * @brief MQTT client event handling callback function
 *
 * This callback function is used to receive MQTT events
 *
 * @param event_id Event ID identifying the type of MQTT event that occurred
 * @param evt_param Event parameter pointer containing event-related data
 * @param user_param User-defined parameter pointer for passing user-specific data
 */
static void unir_mqtt_demo_event_cb(qcm_mqtt_client_event_e event_id, void *evt_param, void *user_param)
{
    mqtt_demo_msg_t msg = {0};
    int             ret = 0;

    QOSA_UNUSED(user_param);
    // Check if event parameter is null, return directly if null
    if (evt_param == QOSA_NULL)
    {
        return;
    }

    QLOGV("event:%d,evt_param:%x", event_id, evt_param);

    // Package message structure, save event information to message
    msg.msg_id = event_id;
    msg.argv = evt_param;

    ret = qosa_msgq_release(g_mqtt_ctx->msgq, sizeof(mqtt_demo_msg_t), (qosa_uint8_t *)&msg, QOSA_NO_WAIT);

    // Check message sending result, log error if sending fails
    if (ret != QOSA_OK)
    {
        QLOGE("ret=%x", ret);
    }
}

/**
 * @brief Open MQTT client connection
 *
 * @return 0 on success, -1 on failure
 */
static int unir_mqtt_demo_open(void)
{
    int               ret = 0;
    qcm_mqtt_config_t mqtt_option = {0};  // MQTT configuration

    /* Initialize MQTT client default configuration */
    ret = qcm_mqtt_client_default_config(&mqtt_option);
    if (ret != QCM_MQTT_RES_OK)
    {
        QLOGE("default config failed!");
        goto exit;
    }

    ret = unir_mqtt_demo_ctx_init(&mqtt_option);
    if (ret != 0)
    {
        QLOGE("init failed!");
        goto exit;
    }

    /* Create MQTT client instance */
    g_mqtt_ctx->client_id = qcm_mqtt_client_create();
    QLOGI("init client_id=%d", g_mqtt_ctx->client_id);

    /* Initialize MQTT client and set event callback function */
    ret = qcm_mqtt_client_init(g_mqtt_ctx->client_id, &mqtt_option, unir_mqtt_demo_event_cb, QOSA_NULL);
    if (ret != QCM_MQTT_RES_OK)
    {
        QLOGE("init failed!");
        goto exit;
    }

    /* Establish MQTT server connection, equivalent to TCP connection */
    ret = qcm_mqtt_client_open(g_mqtt_ctx->client_id, MQTT_DEMO_SERVER_ADDR, g_mqtt_ctx->port);
    if (ret != QCM_MQTT_RES_OK)
    {
        QLOGE("open failed!");
        goto exit;
    }

    return 0;
exit:
    return -1;
}

/**
 * @brief Connect to MQTT server
 *
 * This function is used to establish a connection between the MQTT client and server,
 * using predefined client ID, username, and password for connection authentication
 * @note Some cloud platforms or servers may require special handling of client_id,
 * modify according to actual situation
 */
static int unir_mqtt_demo_connect(void)
{
    qcm_mqtt_eercode_e ret = 0;  // Store MQTT connection operation return value

    QLOGV("enter");
    /* Call MQTT client connection interface, pass connection parameters including client ID, username, password, etc., equivalent to CONNECT control packet connection */
    ret = qcm_mqtt_client_connect(
        g_mqtt_ctx->client_id,
        MQTT_DEMO_CLIENTID,
        qosa_strlen(MQTT_DEMO_CLIENTID),
        MQTT_DEMO_USERNAME,
        qosa_strlen(MQTT_DEMO_USERNAME),
        MQTT_DEMO_PASSWARD,
        qosa_strlen(MQTT_DEMO_PASSWARD)
    );

    /* Check connection result, log error and return error code if connection fails */
    if (ret != QCM_MQTT_RES_OK)
    {
        QLOGE("connect fail=%x", ret);
        return -1;
    }
    return 0;
}

/**
 * @brief MQTT publish message demo function
 *
 * This function demonstrates the MQTT client's message publishing functionality.
 * The function constructs an MQTT message and publishes it to the specified topic.
 *
 * @note When Qos level is 0, the packet identifier field does not exist
 */
static void unir_mqtt_demo_publish(void)
{
    qcm_mqtt_quality_of_service_e qos = QCM_MQTT_AT_LEAST_ONCE_DELIVERY;  // Qos level
    qosa_bool_t                   retain = QOSA_FALSE;                    // Retain message
    qcm_mqtt_eercode_e            ret = 0;
    qcm_mqtt_pub_config_t         pub_info = {0};

    QLOGV("publish");
    // Increment packet ID and execute MQTT message publish operation
    pub_info.msg_id = g_mqtt_ctx->msg_id++;
    pub_info.qos = qos;
    pub_info.retain = retain;
    pub_info.topic.data_ptr = MQTT_DEMO_TOPIC1;
    pub_info.topic.data_len = qosa_strlen(MQTT_DEMO_TOPIC1);
    pub_info.payload.data_ptr = (char *)MQTT_DEMO_TEST_CONTENT;
    pub_info.payload.data_len = qosa_strlen(MQTT_DEMO_TEST_CONTENT);

    ret = qcm_mqtt_client_publish(g_mqtt_ctx->client_id, &pub_info);

    // Check publish operation result, log error information if failed
    if (ret != QCM_MQTT_RES_OK)
    {
        QLOGE("publish err=%x", ret);
    }
}

/**
 * @brief MQTT client subscribe topic test
 *
 * @note Multiple topics can be added in one subscription, there's no explicit limit in the protocol, but it's not recommended to subscribe to too many topics at once
 */
static void unir_mqtt_demo_subscribe(void)
{
    qcm_mqtt_client_info_t client_info = {0};                        // Initialize MQTT client info structure
    qcm_mqtt_sub_topic_t   sub_topic[QCM_MQTT_MAX_TOPIC_NUM] = {0};  // Initialize topic array
    int                    i, topic_cnt = 0;                         // Subscribe topic counter
    qcm_mqtt_eercode_e     ret = 0;                                  // Subscribe operation return value
    qcm_mqtt_sub_config_t  sub_info = {0};

    // Check MQTT client connection state, return directly if not connected
    if (qcm_mqtt_client_get_state_info(g_mqtt_ctx->client_id, &client_info) != QCM_MQTT_RES_OK || client_info.client_state != QCM_MQTT_STATE_MQTT_CONNECTED)
    {
        QLOGE("state err");
        return;
    }

    // Set three topics to subscribe and their QoS levels
    topic_cnt = 0;

    qcm_mqtt_malloc_data(&sub_topic[topic_cnt].topic, qosa_strlen(MQTT_DEMO_TOPIC1), MQTT_DEMO_TOPIC1);
    sub_topic[topic_cnt].qos = QCM_MQTT_AT_LEAST_ONCE_DELIVERY;

    topic_cnt += 1;
    qcm_mqtt_malloc_data(&sub_topic[topic_cnt].topic, qosa_strlen(MQTT_DEMO_TOPIC2), MQTT_DEMO_TOPIC2);
    sub_topic[topic_cnt].qos = QCM_MQTT_AT_LEAST_ONCE_DELIVERY;

    topic_cnt += 1;
    qcm_mqtt_malloc_data(&sub_topic[topic_cnt].topic, qosa_strlen(MQTT_DEMO_TOPIC3), MQTT_DEMO_TOPIC3);
    sub_topic[topic_cnt].qos = QCM_MQTT_AT_LEAST_ONCE_DELIVERY;

    // Increment packet ID and execute subscribe operation
    topic_cnt += 1;
    sub_info.msg_id = g_mqtt_ctx->msg_id++;
    sub_info.topics = sub_topic;
    sub_info.topic_cnt = topic_cnt;
    ret = qcm_mqtt_client_subscribe(g_mqtt_ctx->client_id, &sub_info);

    QLOGD("topic_cnt=%d,err=%x", topic_cnt, ret);
    // Free topic memory
    for (i = 0; i < topic_cnt + 1; i++)
    {
        qcm_mqtt_free_data(&sub_topic[i].topic);
    }

    // Check subscribe operation result, log error and return if failed
    if (ret != QCM_MQTT_RES_OK)
    {
        QLOGE("ret=%x", ret);
    }

    return;
}

/**
 * @brief MQTT client unsubscribe topic demo function
 *
 * This function will unsubscribe from MQTT_DEMO_TOPIC1 and MQTT_DEMO_TOPIC2 topics
 */
static void unir_mqtt_demo_unsubscribe(void)
{
    qcm_mqtt_eercode_e    ret = 0;                // Define MQTT operation return code
    int                   i, topic_cnt = 0;       // Subscribe topic counter
    qcm_mqtt_sub_topic_t  un_sub_topic[2] = {0};  // Initialize topic array
    qcm_mqtt_sub_config_t un_sub_info = {0};

    /* Set topics to unsubscribe from */
    topic_cnt = 0;
    qcm_mqtt_malloc_data(&un_sub_topic[topic_cnt].topic, qosa_strlen(MQTT_DEMO_TOPIC1), MQTT_DEMO_TOPIC1);
    un_sub_topic[topic_cnt].qos = QCM_MQTT_AT_LEAST_ONCE_DELIVERY;

    topic_cnt += 1;
    qcm_mqtt_malloc_data(&un_sub_topic[topic_cnt].topic, qosa_strlen(MQTT_DEMO_TOPIC2), MQTT_DEMO_TOPIC2);
    un_sub_topic[topic_cnt].qos = QCM_MQTT_AT_LEAST_ONCE_DELIVERY;

    /* Execute unsubscribe operation */
    un_sub_info.msg_id = g_mqtt_ctx->msg_id++;
    un_sub_info.topics = un_sub_topic;
    un_sub_info.topic_cnt = topic_cnt;
    ret = qcm_mqtt_client_unsubscribe(g_mqtt_ctx->client_id, &un_sub_info);
    if (ret != QCM_MQTT_RES_OK)
    {
        QLOGE("ret=%x", ret);
    }

    // Free topic memory
    for (i = 0; i < topic_cnt + 1; i++)
    {
        qcm_mqtt_free_data(&un_sub_topic[i].topic);
    }
    return;
}

/**
 * @brief Disconnect MQTT client connection
 *
 * @note The client will send a disconnect packet, indicating a normal disconnection, which will not trigger the will message
 */
static void unir_mqtt_demo_disconnect(void)
{
    qcm_mqtt_eercode_e ret = 0;

    /* Call MQTT client disconnect interface */
    ret = qcm_mqtt_client_disconnect(g_mqtt_ctx->client_id, QOSA_NULL);
    if (ret != QCM_MQTT_RES_OK)
    {
        QLOGE("ret=%x", ret);
    }
}

/**
 * @brief Close MQTT client connection
 *
 * @note close means disconnecting the TCP connection, if you don't want a normal disconnection, you can skip disconnect
 * @note Choose between disconnect and close interfaces based on business needs
 */
static void unir_mqtt_demo_close(void)
{
    qcm_mqtt_eercode_e ret = 0;

    /* Call MQTT client close interface and check the return result */
    ret = qcm_mqtt_client_close(g_mqtt_ctx->client_id);
    if (ret != QCM_MQTT_RES_OK)
    {
        QLOGE("ret=%x", ret);
    }
}

/**
 * @brief MQTT event handling function, used to process various event notifications from MQTT client
 *
 * This function processes different MQTT operation results based on the incoming message type (msg_id),
 * including responses to connection, publish, subscribe operations, and generates corresponding URC reports
 *
 * @param msg Pointer to MQTT message structure, containing event ID and parameters
 *            Parameter content varies depending on msg_id, typically various response structures
 *
 * @return 0 on success, -1 or other error codes on failure
 */
static int unir_mqtt_demo_event_handle(mqtt_demo_msg_t *msg)
{
    qosa_uint8_t client_id = 0;        // Client ID, used to identify current MQTT connection
    char         resp_str[128] = {0};  // Store generated URC response string
    int          resp_len = 0;         // Response string length
    int          ret = 0;              // Function return value

    if (msg == QOSA_NULL)
    {
        return -1;
    }
    qcm_mqtt_common_resp_t *resp_ptr = (qcm_mqtt_common_resp_t *)msg->argv;
    client_id = resp_ptr->client_id;
    QLOGV("msg->msg_id=%d, client_id=%d", msg->msg_id, client_id);
    // Process different MQTT events based on message ID
    if (msg->msg_id == QCM_MQTT_CLIENT_OPEN_EVENT)
    {
        QLOGV("state open");
        resp_len = qosa_snprintf(resp_str, sizeof(resp_str), "+QMTOPEN: %d,%x", client_id, resp_ptr->result);

        // If TCP connection is successful, try to perform connect connection
        if (resp_ptr->result == QCM_MQTT_RES_OK)
        {
            ret = unir_mqtt_demo_connect();
            if (ret != 0)
            {
                QLOGE("connect err");
            }
        }
    }
    else if (msg->msg_id == QCM_MQTT_CLIENT_CONNECT_EVENT)
    {
        QLOGV("state connect");

        // connect result returned
        if (resp_ptr->result == QCM_MQTT_RES_OK)
        {
            resp_len = qosa_snprintf(resp_str, sizeof(resp_str), "+QMTCONN: %d,%d,%d", client_id, 0, resp_ptr->pocotrol_code);
        }
        else
        {
            resp_len = qosa_snprintf(resp_str, sizeof(resp_str), "+QMTCONN: %d,%x", client_id, (resp_ptr->result));
        }

        // After successful connection, try subscription test
        if (resp_ptr->result == QCM_MQTT_RES_OK)
        {
            unir_mqtt_demo_subscribe();
        }
    }
    else if (msg->msg_id == QCM_MQTT_CLIENT_PUBLISH_EVENT)
    {
        // Publish message result returned
        QLOGV("state publish");
        qcm_mqtt_send_resp_t *send_ptr = (qcm_mqtt_send_resp_t *)(resp_ptr->data);

        QLOGD("msg_id:%d, send_result=%d", resp_ptr->msg_id, send_ptr->send_result);

        if (send_ptr->send_result == QCM_MQTT_SEND_OK)
        {
            resp_len = qosa_snprintf(resp_str, sizeof(resp_str), "+QMTPUB: %d,%d,%d", client_id, send_ptr->msg_id, 0);
        }
    }
    else if (msg->msg_id == QCM_MQTT_CLIENT_SUBSCRIBE_EVENT)
    {
        // Subscribe message result returned
        QLOGV("state subscribe =%d", resp_ptr->result);

        qcm_mqtt_send_resp_t *send_ptr = (qcm_mqtt_send_resp_t *)resp_ptr->data;
        if (send_ptr != QOSA_NULL && send_ptr->send_result == QCM_MQTT_SEND_OK)
        {
            resp_len = qosa_snprintf(resp_str, sizeof(resp_str), "+QMTSUB: %d,%d,%d", client_id, resp_ptr->msg_id, send_ptr->retry_count);
        }
        else
        {
            resp_len = qosa_snprintf(resp_str, sizeof(resp_str), "+QMTSUB: %d,%d", client_id, resp_ptr->msg_id);
        }
    }
    else if (msg->msg_id == QCM_MQTT_CLIENT_SUBACK_EVENT)
    {
        // Subscribe message result returned
        QLOGV("state subscribe =%d", resp_ptr->result);
        qcm_mqtt_suback_resp_t *suback_ptr = (qcm_mqtt_suback_resp_t *)resp_ptr->data;

        if (resp_ptr->result == QCM_MQTT_RES_OK)
        {
            resp_len = qosa_snprintf(resp_str, sizeof(resp_str), "+QMTSUB: %d,%d,%d", client_id, resp_ptr->msg_id, resp_ptr->result);
            if (suback_ptr != QOSA_NULL)
            {
                QLOGI("qos_cnt:%d", suback_ptr->qos_cnt);
                qosa_free(suback_ptr->qoss);  //!!! need free
            }

            // After successful subscription, try to publish message
            unir_mqtt_demo_publish();
        }
    }
    else if (msg->msg_id == QCM_MQTT_CLIENT_UNSUBSCRIBE_EVENT)
    {
        // Unsubscribe message result returned
        QLOGV("state unsubscribe=%d", resp_ptr->result);
        qcm_mqtt_send_resp_t *send_ptr = (qcm_mqtt_send_resp_t *)resp_ptr->data;
        if (send_ptr != QOSA_NULL && send_ptr->send_result == QCM_MQTT_SEND_OK)
        {
            resp_len += qosa_snprintf(resp_str, sizeof(resp_str), "+QMTUNS: %d,%d,%d", client_id, resp_ptr->msg_id, send_ptr->retry_count);
        }
        else
        {
            resp_len += qosa_snprintf(resp_str, sizeof(resp_str), "+QMTUNS: %d,%d", client_id, resp_ptr->result);
        }
    }
    else if (msg->msg_id == QCM_MQTT_CLIENT_UNSUBACK_EVENT)
    {
        // Unsubscribe message result returned
        QLOGV("state unsubscribe=%d", resp_ptr->result);
        if (resp_ptr->result == QCM_MQTT_RES_OK)
        {
            resp_len += qosa_snprintf(resp_str, sizeof(resp_str), "+QMTUNS: %d,%d", client_id, resp_ptr->result);
            // Disconnect test
            unir_mqtt_demo_disconnect();
        }
    }
    else if (msg->msg_id == QCM_MQTT_CLIENT_STATE_EVENT)
    {
        // Disconnect event report, can confirm disconnection reason based on result code
        QLOGV("state state");
        qcm_mqtt_close_cause_t *close_cause_ptr = (qcm_mqtt_close_cause_t *)resp_ptr->data;
        if (close_cause_ptr != QOSA_NULL)
        {
            resp_len = qosa_snprintf(resp_str, sizeof(resp_str), "+QMTSTAT: %d,%d", client_id, close_cause_ptr->close_cause);
        }
    }
    else if (msg->msg_id == QCM_MQTT_CLIENT_DISCONNECT_EVENT)
    {
        // disconnect event
        QLOGE("state disconnect");

        resp_len = qosa_snprintf(resp_str, sizeof(resp_str), "+QMTDISC: %d,%x", client_id, resp_ptr->pocotrol_code);
    }
    else if (msg->msg_id == QCM_MQTT_CLIENT_CLOSE_EVENT)
    {
        // close event
        QLOGV("state close");
        if (resp_ptr->result == QCM_MQTT_RES_OK)
        {
            resp_len += qosa_snprintf(resp_str, sizeof(resp_str), "+QMTCLOSE: %d,%d", client_id, 0);
        }
        else
        {
            resp_len = qosa_snprintf(resp_str, sizeof(resp_str), "+QMTCLOSE: %d,%x", client_id, (resp_ptr->result));
        }
    }

    // report urc
    if (resp_len != 0 && MQTT_DEMO_TEST_URC_ENABLE)
    {
        qcm_urc_send_report(QOSA_DEV_SIOLIB_UART1_PORT, resp_str, resp_len, QOSA_URC_OUT_METHOD_UNICAST, QOSA_RI_CATEG_OTHERS, QOSA_URC_SEND_DEFAULT);
        qcm_urc_flush_cache();
    }

    if (resp_ptr->data != QOSA_NULL)
    {
        qosa_free(resp_ptr->data);
    }
    qosa_free(resp_ptr);

    return ret;
}

/**
 * @brief Read MQTT received messages
 */
static void unir_mqtt_demo_recv_new_message(mqtt_demo_msg_t *msg)
{
    qosa_uint8_t               client_id = 0;            // Client ID
    qosa_uint32_t              payload_len = 0;          // Message length
    qcm_mqtt_recv_pub_t        recv_pub = {0};           // Topic
    qcm_mqtt_new_msg_notify_t *new_msg_ptr = QOSA_NULL;  // New message notification structure
    qosa_uint8_t               store_id = 0;             // Store ID
    char                       urc_buf[128] = {0};       // URC buffer

    // Get client ID and free notification message structure memory
    qcm_mqtt_common_resp_t *resp_ptr = (qcm_mqtt_common_resp_t *)msg->argv;
    client_id = resp_ptr->client_id;

    new_msg_ptr = (qcm_mqtt_new_msg_notify_t *)resp_ptr->data;
    store_id = new_msg_ptr->store_id;
    QLOGD("client_id=%d, new_message=%d", client_id, store_id);
    // Read subscribed message content from MQTT client
    if (qcm_mqtt_client_read_subcribe_message(client_id, store_id, &recv_pub) == QCM_MQTT_RES_OK)
    {
        // If URC output is enabled, send message related information via URC
        if (MQTT_DEMO_TEST_URC_ENABLE)
        {
            payload_len = recv_pub.payload.data_len;
            // Construct and send URC header information
            qosa_snprintf(urc_buf, sizeof(urc_buf), "+QMTRECV: %d,%d,\"%s\",%d,\"", client_id, recv_pub.msg_id, recv_pub.topic.data_ptr, payload_len);
            qcm_urc_send_report(
                QOSA_DEV_SIOLIB_UART1_PORT,
                urc_buf,
                qosa_strlen(urc_buf),
                QOSA_URC_OUT_METHOD_UNICAST,
                QOSA_RI_CATEG_OTHERS,
                QOSA_URC_SEND_DEFAULT
            );

            // Send message body content
            qcm_urc_send_report(
                QOSA_DEV_SIOLIB_UART1_PORT,
                recv_pub.payload.data_ptr,
                payload_len,
                QOSA_URC_OUT_METHOD_UNICAST,
                QOSA_RI_CATEG_OTHERS,
                QOSA_URC_SEND_DEFAULT
            );

            // Flush URC cache to ensure data is sent
            qcm_urc_flush_cache();
        }

        // Check for preset topic and unsubscribe if found
        if (qosa_memcmp(MQTT_DEMO_TOPIC1, recv_pub.topic.data_ptr, qosa_strlen(MQTT_DEMO_TOPIC1)) == 0)
        {
            unir_mqtt_demo_unsubscribe();
        }
        // Free topic and payload memory
        qcm_mqtt_free_data(&recv_pub.topic);
        qcm_mqtt_free_data(&recv_pub.payload);
    }

    if (resp_ptr->data != QOSA_NULL)
    {
        qosa_free(resp_ptr->data);
    }
    qosa_free(resp_ptr);
}

/**
 * @brief MQTT demo task main function, responsible for initializing MQTT client and handling various MQTT events
 */
static void unir_mqtt_demo_task(void *argv)
{
    int                    ret = 0;     // Function return value
    mqtt_demo_msg_t        msg = {0};   // MQTT message structure
    qcm_mqtt_client_info_t info = {0};  // MQTT client information structure
    QOSA_UNUSED(argv);

    // Wait for system initialization to complete and facilitate log debugging
    qosa_task_sleep_sec(10);
    ret = unir_mqtt_demo_open();
    if (ret != 0)
    {
        goto exit;
    }

    // Main event processing loop
    while (1)
    {
        qosa_memset(&msg, 0, sizeof(mqtt_demo_msg_t));
        qosa_msgq_wait(g_mqtt_ctx->msgq, (qosa_uint8_t *)&msg, sizeof(mqtt_demo_msg_t), QOSA_WAIT_FOREVER);
        QLOGD("msg_id=%d", msg.msg_id);

        // Dispatch and process different events based on message ID
        switch (msg.msg_id)
        {
            case QCM_MQTT_CLIENT_OPEN_EVENT:
                ret = unir_mqtt_demo_event_handle(&msg);
                break;
            case QCM_MQTT_CLIENT_CONNECT_EVENT:
                ret = unir_mqtt_demo_event_handle(&msg);
                break;
            case QCM_MQTT_CLIENT_PUBLISH_EVENT:
                ret = unir_mqtt_demo_event_handle(&msg);
                break;
            case QCM_MQTT_CLIENT_SUBSCRIBE_EVENT:
                ret = unir_mqtt_demo_event_handle(&msg);
                break;
            case QCM_MQTT_CLIENT_SUBACK_EVENT:
                ret = unir_mqtt_demo_event_handle(&msg);
                break;
            case QCM_MQTT_CLIENT_UNSUBSCRIBE_EVENT:
                ret = unir_mqtt_demo_event_handle(&msg);
                break;
            case QCM_MQTT_CLIENT_UNSUBACK_EVENT:
                ret = unir_mqtt_demo_event_handle(&msg);
                break;
            case QCM_MQTT_CLIENT_NEW_MESSAGE_EVENT:
                unir_mqtt_demo_recv_new_message(&msg);
                break;
            case QCM_MQTT_CLIENT_STATE_EVENT:
                ret = unir_mqtt_demo_event_handle(&msg);
                break;
            case QCM_MQTT_CLIENT_DISCONNECT_EVENT:
                ret = unir_mqtt_demo_event_handle(&msg);
                break;
            case QCM_MQTT_CLIENT_CLOSE_EVENT:
                ret = unir_mqtt_demo_event_handle(&msg);
                break;
            default:
                break;
        }

        // Get and record current MQTT client state
        qosa_memset(&info, 0, sizeof(qcm_mqtt_client_info_t));
        qcm_mqtt_client_get_state_info(g_mqtt_ctx->client_id, &info);
        QLOGD("state=%d", info.client_state);

        // The following close is just for convenience of single-threaded demo, actual closing is determined by the user
        // Decide whether to close the client based on state
        if (info.client_state == QCM_MQTT_STATE_TCP_CLOSING || info.client_state == QCM_MQTT_STATE_MQTT_DISCONNECTING)
        {
            unir_mqtt_demo_close();
        }

        // If connection is closed or in initial state, exit task
        if (info.client_state == QCM_MQTT_STATE_TCP_CLOSED || info.client_state == QCM_MQTT_STATE_INIT)
        {
            goto exit;
        }

        // If error occurs during processing, also exit task
        if (ret != 0)
        {
            goto exit;
        }
    }

exit:
    QLOGV("end");
    // Free resources
    unir_mqtt_demo_ctx_free();
}

/**
 * @note This demonstrates the basic usage of MQTT API, URC is just for convenient debugging and status printing, MQTTS testing can be selected via MQTT_DEMO_TEST_URC_ENABLE
 */
void unir_mqtt_demo_init(void)
{
    int         err = 0;
    qosa_task_t mqtt_task = QOSA_NULL;

    err = qosa_task_create(&mqtt_task, MQTT_DEMO_TASK_STACK_SIZE, QOSA_PRIORITY_NORMAL, "mqtt_demo", unir_mqtt_demo_task, QOSA_NULL);
    if (err != QOSA_OK)
    {
        QLOGE("task create error");
        return;
    }
}