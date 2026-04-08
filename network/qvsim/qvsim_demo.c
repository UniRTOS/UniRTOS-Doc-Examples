/*****************************************************************/ /**
* @file qvsim_demo.c
* @brief QVSIM demo implementation
* @author joe.tu@quectel.com
* @date 2025-12-08
*
* @copyright Copyright (c) 2023 Quectel Wireless Solution, Co., Ltd.
* All Rights Reserved. Quectel Wireless Solution Proprietary and Confidential.
*
* @par EDIT HISTORY FOR MODULE
* <table>
* <tr><th>Date <th>Version <th>Author <th>Description
* <tr><td>2025-12-08 <td>1.0 <td>joe.tu <td> Init
* </table>
**********************************************************************/

#include "qosa_def.h"
#include "qosa_sys.h"
#include "qosa_qvsim.h"
#include "qosa_log.h"
#include "qosa_sim.h"
#include "qosa_event_notify.h"
#include "qvsim_demo.h"

#define QOS_LOG_TAG                         LOG_TAG

#define QVSIM_DEMO_EVENT_INIT_DONE          QOSA_QVSIM_EVENT_INIT_DONE
#define QVSIM_DEMO_EVENT_SELECT_PROFILE     QOSA_QVSIM_EVENT_SELECT_PROFILE
#define QVSIM_DEMO_EVENT_START_PROFILE      QOSA_QVSIM_EVENT_START_PROFILE
#define QVSIM_DEMO_EVENT_SWITCH_IR          QOSA_QVSIM_EVENT_SWITCH_IR
#define QVSIM_DEMO_EVENT_OTA_ADD_PROFILE    QOSA_QVSIM_EVENT_OTA_ADD_PROFILE
#define QVSIM_DEMO_EVENT_OTA_DELETE_PROFILE QOSA_QVSIM_EVENT_OTA_DELETE_PROFILE
#define QVSIM_DEMO_EVENT_OTA_SWITCH_PROFILE QOSA_QVSIM_EVENT_OTA_SWITCH_PROFILE
#define QVSIM_DEMO_EVENT_OTA_NO_TASK        QOSA_QVSIM_EVENT_OTA_NO_TASK
#define QVSIM_DEMO_EVENT_OTA_CONNECTED      QOSA_QVSIM_EVENT_OTA_CONNECTED
#define QVSIM_DEMO_EVENT_OTA_FINISHED       QOSA_QVSIM_EVENT_OTA_FINISHED
#define QVSIM_DEMO_EVENT_OTA_ERROR          QOSA_QVSIM_EVENT_OTA_ERROR
#define QVSIM_DEMO_EVENT_SIM_PIN_READY      (QOSA_QVSIM_EVENT_OTA_ERROR + 100)

/** qvsim demo task handle */
static qosa_task_t g_qvsim_demo_task = QOSA_NULL;

/** qvsim demo message queue handle */
static qosa_msgq_t g_qvsim_demo_queue = QOSA_NULL;

// ===========================================================================
// [Data Structure] Define messages for communication between Callback and MainTask
// ===========================================================================

/**
 * @struct demo_qvsim_msg_t
 * @brief QVSIM demo message structure for communication between callback and main task
 */
typedef struct
{
    qosa_int32_t event_id; /*!< Event type */

    /*!< Union used to carry data for different events */
    /*!< Note: Pointer data in Callback (such as strings) must be deep copied here, cannot directly pass pointers */
    union
    {
        qosa_int32_t result_code;   /*!< Used to store various int type result/error */
        char         text_buff[32]; /*!< Used to store short strings such as iccid, avoiding malloc */
        struct
        {
            qosa_int32_t code; /*!< Extended error code */
            qosa_int32_t type; /*!< Extended error type */
        } extended_info;       /*!< Extended information structure */
    } data;                    /*!< Union data for different events */

} demo_qvsim_msg_t;

/**
 * @brief SIM card status event callback function
 * 
 * @param [in] user_argv
 *           - User-defined parameters, not used in this function.
 * @param [in] argv
 *           - The event parameter pointer points to the qosa_sim_status_event_t structure.
 * 
 * @return A return value of 0 indicates successful processing.
 */
static int demo_sim_status_event_callback(void *user_argv, void *argv)
{
    demo_qvsim_msg_t         msg = {0};
    qosa_sim_status_event_t *ev = (qosa_sim_status_event_t *)argv;
    QLOGI("[SIM EVENT] SIM%d Status Changed: %d cause:%d", ev->simid, ev->status, ev->cause);

    // Check SIM card status, if it is ready state
    if (QOSA_SIM_STATUS_READY == ev->status && ev->cause == 0)
    {
        QLOGI("[SIM EVENT] SIM%d is ready", ev->simid);

        msg.event_id = QVSIM_DEMO_EVENT_SIM_PIN_READY;
        if (g_qvsim_demo_queue != QOSA_NULL)
        {
            QLOGI(" just send event to main task");
            int ret = qosa_msgq_release(g_qvsim_demo_queue, sizeof(msg), (qosa_uint8_t *)&msg, QOSA_NO_WAIT);
            QLOGI(" qosa_msgq_release return: %d", ret);
        }
    }
    return 0;
}

// ===========================================================================
// [Producer] Callback functions
// Features: Run in underlying threads/interrupts, only responsible for packaging messages and sending them to the queue, strictly no blocking
// ===========================================================================

/**
 * @brief QVSIM event callback function
 * 
 * This function handles QVSIM events, packages the event data into a message structure,
 * and sends it to the message queue for processing by the main task.
 * 
 * @param [in] event
 *           - QVSIM event type
 * @param [in] event_data
 *           - Pointer to event-specific data
 * @param [in] user_data
 *           - User-defined data, not used in this function
 * 
 * @return No return value
 */
static void demo_qvsim_event_callback(qosa_qvsim_event_e event, void *event_data, void *user_data)
{
    demo_qvsim_msg_t msg;
    qosa_memset(&msg, 0, sizeof(msg));
    msg.event_id = event;

    // --- Data packaging (Deep Copy) ---
    switch (event)
    {
        case QOSA_QVSIM_EVENT_START_PROFILE: {
            qosa_qvsim_result_start_profile_t *p = (qosa_qvsim_result_start_profile_t *)event_data;
            msg.data.result_code = p->result;
            break;
        }
        case QOSA_QVSIM_EVENT_SELECT_PROFILE: {
            qosa_qvsim_result_select_profile_t *p = (qosa_qvsim_result_select_profile_t *)event_data;
            msg.data.result_code = p->result;
            break;
        }
        case QOSA_QVSIM_EVENT_OTA_FINISHED: {
            qosa_qvsim_ota_event_data_t *p = (qosa_qvsim_ota_event_data_t *)event_data;
            msg.data.result_code = p->data.finished.result;
            break;
        }
        case QOSA_QVSIM_EVENT_OTA_ERROR: {
            qosa_qvsim_ota_event_data_t *p = (qosa_qvsim_ota_event_data_t *)event_data;
            msg.data.result_code = p->data.error.err_code;
            break;
        }
        // Events involving strings require content copying to prevent pointer invalidation after Callback returns
        case QOSA_QVSIM_EVENT_OTA_ADD_PROFILE:
        case QOSA_QVSIM_EVENT_OTA_SWITCH_PROFILE:
        case QOSA_QVSIM_EVENT_OTA_DELETE_PROFILE: {
            qosa_qvsim_ota_event_data_t *p = (qosa_qvsim_ota_event_data_t *)event_data;
            // Protective copy to prevent overflow
            qosa_strncpy(msg.data.text_buff, p->data.profile.iccid, sizeof(msg.data.text_buff) - 1);
            break;
        }
        default:
            // Other events that do not require data (such as CONNECTED, NO_TASK)
            break;
    }

    // --- Send message ---
    // Use NO_WAIT because blocking is not allowed in callbacks. If the queue is full, it is usually chosen to discard or print warnings.
    if (g_qvsim_demo_queue != QOSA_NULL)
    {
        qosa_msgq_release(g_qvsim_demo_queue, sizeof(msg), (qosa_uint8_t *)&msg, QOSA_NO_WAIT);
    }
}

// ===========================================================================
// [Business Logic] Specific functional implementation functions
// ===========================================================================

/**
 * @brief Display QVSIM device information
 * 
 * This function retrieves and displays the device UID and VSIM version information.
 * 
 * @param None
 * 
 * @return No return value
 */
static void business_show_device_info(void)
{
    char buf[64] = {0};
    if (qosa_qvsim_get_uid(buf, sizeof(buf)) == QOSA_QVSIM_ERR_OK)
    {
        QLOGI("Device UID: %s", buf);
    }
    if (qosa_qvsim_get_version(buf, sizeof(buf)) == QOSA_QVSIM_ERR_OK)
    {
        QLOGI("VSIM Version: %s", buf);
    }
}

/**
 * @brief List all available QVSIM profiles
 * 
 * This function retrieves and displays all available QVSIM profiles,
 * including their slot numbers and ICCID values.
 * 
 * @param None
 * 
 * @return No return value
 */
static void business_list_profiles(void)
{
    qosa_qvsim_profile_t profiles[10];
    qosa_uint8_t         count = 0;
    int                  i = 0;

    if (qosa_qvsim_list_profiles(profiles, &count, 10) == QOSA_QVSIM_ERR_OK)
    {
        QLOGI("Profile Count: %d", count);
        for (i = 0; i < count; i++)
        {
            QLOGI("  [%d] Slot:%d ICCID:%s", i, profiles[i].slot, profiles[i].iccid);
        }
    }
    else
    {
        QLOGI("Failed to list profiles");
    }
}

/**
 * @brief Trigger QVSIM OTA process
 * 
 * This function configures OTA parameters and starts the OTA process
 * to download and install QVSIM profiles.
 * 
 * @param None
 * 
 * @return No return value
 */
static void business_trigger_ota(void)
{
    // Configure OTA parameters (example)
    qosa_qvsim_ota_config_t cfg = {0};
    qosa_strncpy(cfg.url, "ota-server.com:443", sizeof(cfg.url) - 1);
    qosa_strncpy(cfg.username, "user", sizeof(cfg.username) - 1);
    qosa_strncpy(cfg.password, "password", sizeof(cfg.password) - 1);
    cfg.ota_time = 120;
    qosa_qvsim_set_ota_config(&cfg);

    QLOGI("Starting OTA Process...");
    qosa_qvsim_err_e ret = qosa_qvsim_start_ota();
    if (ret != QOSA_QVSIM_ERR_OK)
    {
        QLOGI("OTA Start Failed API Return: 0x%X", ret);
    }
}

// ===========================================================================
// [Consumer] Demo main task
// Features: Has independent stack space, can safely handle complex logic, block waiting for messages
// ===========================================================================

/**
 * @brief QVSIM demo task main function
 * 
 * This function is the main entry point for the QVSIM demo task. It initializes
 * the message queue, registers callbacks, and enters an event loop to handle
 * QVSIM events and SIM status changes.
 * 
 * @param [in] arg
 *           - Task parameter pointer, not used in this function
 * 
 * @return No return value
 */
void qvsim_demo_task_entry(void *arg)
{
    QOSA_UNUSED(arg);
    int              ret = 0;
    demo_qvsim_msg_t msg;

    QLOGI("Initializing QVSIM Queue Demo...");

    // 1. Create message queue (depth is 10, each message size is demo_qvsim_msg_t)
    ret = qosa_msgq_create(&g_qvsim_demo_queue, sizeof(demo_qvsim_msg_t), 10);
    if (ret != QOSA_OK)
    {
        QLOGE("Create qvsim demo queue failed: %d", ret);
        return;
    }

    // 2. Register callbacks
    qosa_qvsim_register_callback(demo_qvsim_event_callback, QOSA_NULL);
    qosa_event_notify_register(QOSA_EVENT_MODEM_SIM_STATUS, demo_sim_status_event_callback, QOSA_NULL);

    business_show_device_info();

    // 4. Event loop (Event Loop)
    while (1)
    {
        // Block waiting for message (Wait Forever) - no CPU idle here
        if (qosa_msgq_wait(g_qvsim_demo_queue, (qosa_uint8_t *)&msg, sizeof(msg), QOSA_WAIT_FOREVER) == 0)  // 0 == OK
        {
            // --- Handle business logic uniformly here ---
            switch (msg.event_id)
            {
                case QOSA_QVSIM_EVENT_INIT_DONE:
                    QLOGI("Event: QVSIM Service Initialized.");

                    qosa_qvsim_status_e status = qosa_qvsim_get_status();
                    QLOGI("Current VSIM Status: %d", status);

                    if (QOSA_QVSIM_STATUS_DISABLED == status)
                    {
                        // Optional: automatically start OTA demonstration
                        business_trigger_ota();
                    }
                    else
                    {
                        // Startup successful, execute post-initialization business
                        business_list_profiles();

                        // Close service demonstration
                        qosa_task_sleep_sec(10);
                        qosa_qvsim_stop();
                    }

                    break;

                case QOSA_QVSIM_EVENT_START_PROFILE:
                    if (msg.data.result_code == 0)
                    {
                        QLOGI(">>> Event: VSIM Service Operate Successfully! <<<");
                        // Startup successful, execute post-initialization business

                        business_list_profiles();
                    }
                    else
                    {
                        QLOGI("!!! Event: VSIM Operate Failed. Code: %d !!!", msg.data.result_code);
                    }
                    break;

                case QOSA_QVSIM_EVENT_OTA_CONNECTED:
                    QLOGI("Event: OTA Server Connected.");
                    break;

                case QOSA_QVSIM_EVENT_OTA_ADD_PROFILE:
                    QLOGI("Event: OTA Added Profile ICCID: %s", msg.data.text_buff);
                    break;

                case QOSA_QVSIM_EVENT_OTA_FINISHED: {
                    QLOGI("Event: OTA Finished. Result: %d", msg.data.result_code);

                    business_list_profiles();
                    // Can trigger subsequent processes here, such as restart or switch Profile

                    // Start service
                    ret = qosa_qvsim_start();
                    QLOGI("Invoked qosa_qvsim_start(), return 0x%X", ret);
                    if (ret != QOSA_QVSIM_ERR_OK)
                    {
                        QLOGI("Failed to invoke start API");
                    }
                    QLOGI("Waiting for events...");
                }
                break;

                case QOSA_QVSIM_EVENT_OTA_ERROR:
                    QLOGI("Event: OTA Error Occurred. ErrCode: %d", msg.data.result_code);
                    break;

                case QVSIM_DEMO_EVENT_SIM_PIN_READY: {
                    QLOGI("Event: SIM PIN Ready.");

                    // Read IMSI
                    qosa_sim_imsi_t imsi = {0};
                    ret = qosa_sim_read_imsi(0, &imsi);
                    if (ret == QOSA_SIM_ERR_OK)
                    {
                        QLOGI("IMSI: %s", imsi.imsi);
                    }
                    else
                    {
                        QLOGE("Failed to read IMSI: 0x%x", ret);
                    }
                    break;
                }

                // ... other events ...
                default:
                    QLOGI("Event: Unhandled event ID %d", msg.event_id);
                    break;
            }
        }
    }
}

/**
 * @brief Initialize QVSIM demo
 * 
 * This function creates the QVSIM demo task if it doesn't exist.
 * The task will automatically start running after creation.
 * 
 * @param None
 * 
 * @return No return value
 * 
 * @note Task stack size is 4KB, priority is low priority
 */
void unir_qvsim_demo_init(void)
{
    if (g_qvsim_demo_task == QOSA_NULL)
    {
        int err = qosa_task_create(&g_qvsim_demo_task, 4 * 1024, QOSA_PRIORITY_LOW, "qvsim_demo", qvsim_demo_task_entry, QOSA_NULL);
        // Check if task creation was successful
        if (err != QOSA_OK)
        {
            QLOGE("qvsim_demo_init task create error");
            return;
        }
    }
}
