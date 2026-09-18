#include "qosa_def.h"
#include "qosa_log.h"
#include "qosa_sim.h"
#include "qosa_dev1.h"
#include "qosa_network.h"
#include "qosa_datacall.h"
#include "qosa_ip_addr.h"
#include "qosa_event_notify.h"

#define QOS_LOG_TAG                       LOG_TAG_DEMO

/** network demo maximum wait time for network attachment, unit: seconds */
#define NW_DEMO_WAIT_ATTACH_MAX_WAIT_TIME (300)
/** network demo maximum execution times for CFUN function */
#define NW_DEMO_EXCUTE_CFUN_MAXTIME       (10)

/**
 * @struct nw_demo_nw_msg_t
 * @brief network demo message structure
 *
 */
typedef struct nw_msg
{
    qosa_uint32_t event_id; /*!< Event ID */
    union
    {
        qosa_nw_reg_status_event_t         reg_status;     /*!< network registration status update */
        qosa_nw_reg_signal_quality_event_t signal_quality; /*!< signal quality update */
        qosa_nw_rrc_conn_event_t           rrc_conn;       /*!< RRC connection update */
        qosa_nw_nitz_event_t               nitz_info;      /*!< NITZ info update */
        qosa_nw_nas_event_t                nas_event_info; /*!< nas event update */
    };

} nw_demo_nw_msg_t;

/** network demo task handle */
qosa_task_t g_nw_task = QOSA_NULL;
/** network demo CFUN operation semaphore */
qosa_sem_t g_nw_demo_cfun_sem = QOSA_NULL;
/** network demo NAS event information */
qosa_nw_nas_event_t g_nw_demo_nas_event = {0};

/**
 * @brief network demo event callback function
 *
 * This function is used to handle network-related events, including PS registration status,
 * signal quality, RRC connection status, NITZ information, and NAS events, etc.
 *
 * @param[in] user_argv
 *          - User parameter, points to event ID
 *
 * @param[in] argv
 *          - Event data pointer, contains different structure data according to event type
 *
 * @return int
 *       - Returns 0 on success, -1 on failure
 */
static int network_event_cb(void *user_argv, void *argv)
{
    qosa_notify_event_e event_id = (qosa_ptr)user_argv;
    nw_demo_nw_msg_t   *msg = QOSA_NULL;

    msg = qosa_malloc(sizeof(nw_demo_nw_msg_t));
    if (!msg)
    {
        QLOGV("msg alloc failed!");
        return -1;
    }

    msg->event_id = event_id;
    QLOGI("nw msg id:%d", msg->event_id);
    switch (event_id)
    {
        case QOSA_EVENT_MODEM_NW_PS_REG_STATUS: {
            // QLOGI("QOSA_EVENT_MODEM_NW_PS_REG_STATUS coming");

            qosa_memcpy(&msg->reg_status, argv, sizeof(qosa_nw_reg_status_event_t));

            QLOGI(
                "simid:%d,reg_status:%d,loc_present:%d,lac:%d,cid:%d,act:%d",
                msg->reg_status.simid,
                msg->reg_status.reg_status,
                msg->reg_status.loc_present,
                msg->reg_status.lac,
                msg->reg_status.cid,
                msg->reg_status.act
            );
            break;
        }
        case QOSA_EVENT_MODEM_NW_SIGNAL_QUALITY: {
            // QLOGI("QOSA_EVENT_MODEM_NW_SIGNAL_QUALITY coming");

            qosa_memcpy(&msg->signal_quality, argv, sizeof(qosa_nw_reg_signal_quality_event_t));

            QLOGI("simid:%d,rat:%d", msg->signal_quality.simid, msg->signal_quality.rat);
            if (msg->signal_quality.rat == QOSA_NW_RAT_4G)
            {
                QLOGI(
                    "rsrp:%d,rsrq:%d,sinr:%d,rssi:%d",
                    msg->signal_quality.lte.rsrp,
                    msg->signal_quality.lte.rsrq,
                    msg->signal_quality.lte.sinr,
                    msg->signal_quality.lte.rssi
                );
                if (((msg->signal_quality.lte.rsrp / 100) <= -115) || ((msg->signal_quality.lte.rsrq / 100) <= -15)
                    || ((msg->signal_quality.lte.sinr / 100) <= 0))
                {
                    QLOGI("Currently in bad network!");
                }
            }
            break;
        }
        case QOSA_EVENT_MODEM_NW_RRC_STATUS: {
            // QLOGI("QOSA_EVENT_MODEM_NW_RRC_STATUS coming");

            qosa_memcpy(&msg->rrc_conn, argv, sizeof(qosa_nw_rrc_conn_event_t));

            QLOGI("simid:%d,mode:%d,state:%d,access:%d", msg->rrc_conn.simid, msg->rrc_conn.mode, msg->rrc_conn.state, msg->rrc_conn.access);
            break;
        }
        case QOSA_EVENT_MODEM_NW_NITZ_INFO: {
            // QLOGI("QOSA_EVENT_MODEM_NW_NITZ_INFO coming");

            qosa_memcpy(&msg->nitz_info, argv, sizeof(qosa_nw_nitz_event_t));

            QLOGI(
                "%d-%d-%d %d-%d-%d,timezone=%d,dst=%d",
                msg->nitz_info.year,
                msg->nitz_info.month,
                msg->nitz_info.day,
                msg->nitz_info.hour,
                msg->nitz_info.minute,
                msg->nitz_info.second,
                msg->nitz_info.timezone,
                msg->nitz_info.dst
            );
            break;
        }
        case QOSA_EVENT_MODEM_NW_NAS_EVENT: {
            // QLOGI("QOSA_EVENT_MODEM_NW_NAS_EVENT coming");

            qosa_memcpy(&msg->nas_event_info, argv, sizeof(qosa_nw_nas_event_t));

            QLOGI("event_type=%d,cause=%d", msg->nas_event_info.event_type, msg->nas_event_info.reject_cause);

            g_nw_demo_nas_event.event_type = msg->nas_event_info.event_type;
            g_nw_demo_nas_event.reject_cause = msg->nas_event_info.reject_cause;
            break;
        }
        default:
            break;
    }

    qosa_free(msg);
    msg = QOSA_NULL;
    return 0;
}
/**
 * @brief network demo set CFUN callback function
 *
 * This function serves as the callback function for qosa_dev_set_cfun API,
 * used to handle the result after CFUN setting is completed
 *
 * @param[in] ctx
 *          - Context parameter
 *
 * @param[in] argv
 *          - CFUN setting result confirmation information pointer
 */
static void nw_demo_set_cfun_cb(void *ctx, void *argv)
{
    QOSA_UNUSED(ctx);
    // qosa_dev_set_cfun API execution completed
    qosa_dev_general_cnf_t *cnf = argv;
    QLOGI("cfun result=%d", cnf->err_code);

    // Release CFUN semaphore, let the program continue to run
    qosa_sem_release(g_nw_demo_cfun_sem);
}
/**
 * @brief Get the rssi used by csq in LTE
 *
 * @param[in] lte_rssi
 *          - rssi in lte
 * @return qosa_uint8_t
 */
static qosa_uint8_t nw_demo_lte_get_csq_rssi(qosa_rssi_t lte_rssi)
{
    qosa_int16_t rssi = 0;
    
    QLOGI("input rssi:%d", lte_rssi);

    // Invalid value converted to 99
    if (QOSA_NW_PI_INT16 == lte_rssi)
    {
        rssi = 99;
    }
    else
    {
        rssi = (QOSA_RSSI_MEAS(lte_rssi) + 113) / 2;
    }

    //range constraint
    rssi = ((rssi == 99) ? 99 : ((rssi > 31) ? 31 : ((rssi > 0) ? rssi : 0)));
    
    QLOGI("lte csq rssi:%d", rssi);
    
    return rssi;
}
/**
 * @brief network demo task main function
 *
 * This function serves as the main loop of the network demo task, implementing the following functions:
 * - Register network demo event callback functions
 * - Check SIM card status and network registration status
 * - Wait for network attachment, if failed then perform troubleshooting
 * - Periodically obtain and display network parameter information
 * - Support CFUN restart function to restore network connection
 *
 * @param[in] arg
 *          - Task parameter
 */
static void nw_demo_task(void *arg)
{
    int                      ret = 0;
    qosa_uint8_t             simid = 0;
    qosa_nw_rat_e            current_rat = 0;
    qosa_bool_t              is_attached = QOSA_FALSE;
    qosa_uint8_t             ps_status;
    qosa_nw_oper_name_t      long_oper_name = {0};
    qosa_nw_oper_name_t      short_oper_name = {0};
    qosa_nw_scell_info_t     scell_info = {0};
    qosa_sim_status_e        status;
    qosa_nw_freq_lock_list_t lock_freq = {0};
    int                      count = 0;
    int                      cfun_count = 0;
    QOSA_UNUSED(arg);

    qosa_task_sleep_sec(3);

    ret = qosa_sem_create(&g_nw_demo_cfun_sem, 0);
    QLOGI("create sem result=%d", ret);
    // Register PS registration status change event callback
    qosa_event_notify_register(QOSA_EVENT_MODEM_NW_PS_REG_STATUS, network_event_cb, (void *)QOSA_EVENT_MODEM_NW_PS_REG_STATUS);
    // Register RRC connection status change event callback
    qosa_event_notify_register(QOSA_EVENT_MODEM_NW_RRC_STATUS, network_event_cb, (void *)QOSA_EVENT_MODEM_NW_RRC_STATUS);
    // Register signal quality change event callback
    qosa_event_notify_register(QOSA_EVENT_MODEM_NW_SIGNAL_QUALITY, network_event_cb, (void *)QOSA_EVENT_MODEM_NW_SIGNAL_QUALITY);
    // Register NITZ information report event callback
    qosa_event_notify_register(QOSA_EVENT_MODEM_NW_NITZ_INFO, network_event_cb, (void *)QOSA_EVENT_MODEM_NW_NITZ_INFO);
    // Register NAS report event callback
    qosa_event_notify_register(QOSA_EVENT_MODEM_NW_NAS_EVENT, network_event_cb, (void *)QOSA_EVENT_MODEM_NW_NAS_EVENT);
    
    // Get current SIM card status to check if there are any issues in the SIM card status
    ret = qosa_sim_read_status(simid, &status);
    QLOGI("ret=%d,status=%d", ret, status);

    // Get supported band configuration
    qosa_nw_band_t support_band = {0};
    ret = qosa_nw_get_support_band(simid, &support_band);
    QLOGI("ret=0x%x, support band:0x%x %x %x", ret, support_band.ltebandval[2], support_band.ltebandval[1], support_band.ltebandval[0]);

    // Get current band configuration
    // Comparing the differences between two band configurations can assist in troubleshooting the issue of attach failure
    qosa_nw_band_t current_band = {0};
    ret = qosa_nw_get_cfg_band(simid, &current_band);
    QLOGI("ret=0x%x, current band:0x%x %x %x", ret, current_band.ltebandval[2], current_band.ltebandval[1], current_band.ltebandval[0]);

    // If attach is successful before the max time timeout, it will immediately return QOSA_TRUE and enter second while loop
    // Otherwise, it will block until timeout and returning QOSA_FALSE and enter first while loop
    is_attached = qosa_datacall_wait_attached(simid, NW_DEMO_WAIT_ATTACH_MAX_WAIT_TIME);

    while (!is_attached)
    {
        // Enter this loop to assist in troubleshooting the issue of attach failure
        QLOGI("attached failed");
        // Task sleep 5 seconds
        qosa_task_sleep_sec(5);

        count++;
    wait_attach:
        // Get current SIM card status to check if there are any issues in the SIM card status
        qosa_sim_read_status(simid, &status);
        QLOGI("ret=%d,status=%d", ret, status);

        // Get current PS registration status, if the current registration status is successful, exit this loop
        ret = qosa_nw_get_reg_status(simid, QOSA_NULL, &ps_status);
        QLOGI("ret=0x%x,ps_status=%d", ret, ps_status);
        if (QOSA_NW_ATTACHED(ps_status))
        {
            break;
        }

        // Try to restart modem function, execute cfun0/1 every 5 minutes, cfun0/1 maximum time is 10
        if ((count % 60 == 0) && cfun_count < NW_DEMO_EXCUTE_CFUN_MAXTIME)
        {
            // Execute cfun0
            ret = qosa_dev_set_cfun(simid, 0, nw_demo_set_cfun_cb, QOSA_NULL);
            QLOGI("ret=%d,qosa_dev_set_cfun", ret);
            // Wait semaphore from nw_demo_set_cfun_cb
            qosa_sem_wait(g_nw_demo_cfun_sem, QOSA_WAIT_FOREVER);

            // Execute cfun1
            ret = qosa_dev_set_cfun(simid, 1, nw_demo_set_cfun_cb, QOSA_NULL);
            QLOGI("ret=%d,qosa_dev_set_cfun", ret);

            // Wait semaphore from nw_demo_set_cfun_cb
            qosa_sem_wait(g_nw_demo_cfun_sem, QOSA_WAIT_FOREVER);

            // Record CFUN execution times
            cfun_count++;
        }
        // Check if there are any NAS events reported
        if (g_nw_demo_nas_event.event_type == QOSA_NW_NAS_ATTACH_REJECT && g_nw_demo_nas_event.reject_cause != 0)
        {
            QLOGI("attach fail,nas event[%d] cause[%d] has been report!", g_nw_demo_nas_event.event_type, g_nw_demo_nas_event.reject_cause);
            /*
            Rejection value                      Description                                                 Possible reasons
            15              No Suitable Cells In tracking area                          SIM card arrears, check SIM card
            33              Requested service option not subscribed                     SIM card does not support LTE, check card
            14              EPS services not allowed in this PLMN                       SIM card does not support LTE, check card
            27              Missing or unknown APN                                      APN mismatch
            30              Request rejected by serving GW or PDN GW                    Frequent power on/off rejected by network
            10              Implicitly detached                                         Implicit detachment, network kicked offline
            55              Multiple PDN connections for a given APN not allowed        Multiple PDP creation rejected, check card
            7               EPS services not allowed                                    Occasional occurrence may be network penalty, try cfun0/1 switching. Frequent occurrence requires checking if SIM card has arrears
            8               EPS services and non-EPS services not allowed               Check if SIM card has arrears
            */
        }
        // Check if the lock frequency/cell function is currently enabled
        qosa_nw_get_freq_lock_config(simid, QOSA_NW_RAT_4G, &lock_freq);
        if (lock_freq.num > 0)
        {
            QLOGI("frequency/cell lock function already activated!!!");
        }

        // Check if the current serving cell PLMN is valid.
        // If it is invalid, it means that it is not currently camped on a cell
        // If it is valid, it means that it is currently camped on a cell, check its signal parameters
        qosa_nw_get_scell_info(simid, &scell_info);
        QLOGI("mcc%d,mnc%d", scell_info.lte.plmn.mcc, scell_info.lte.plmn.mnc);
        if (QOSA_FALSE == QOSA_NW_PLMN_IS_VALID(&scell_info.lte.plmn))
        {
            QLOGI("attach fail, currently not camped on a cell!");
        }
        else
        {
            QLOGI("tac=%x, pcid=%d, earfcn=%d", scell_info.lte.tac, scell_info.lte.pcid, scell_info.lte.earfcn);

            QLOGI("rsrp=%d,rsrq=%d,sinr=%d", scell_info.lte.rsrp, scell_info.lte.rsrq, scell_info.lte.sinr);

            // Can be judged as bad network: rsrp <= -115 dBm or rsrq <= -15 dB or sinr <= 0 dB
            if (((scell_info.lte.rsrp / 100) <= -115) || ((scell_info.lte.rsrq / 100) <= -15) || ((scell_info.lte.sinr / 100) <= 0))
            {
                QLOGI("Currently in bad network! May cause registration failure");
            }
        }
    }
    g_nw_demo_nas_event.event_type = 0;
    g_nw_demo_nas_event.reject_cause = 0;
    while (1)
    {
        // If attach successful, execute loop once every 10 seconds to get network parameters
        qosa_task_sleep_sec(10);
        QLOGI("====================nw demo ===================");

        // Get current PS registration status, if the current registration status is not successful, go to the first loop
        ret = qosa_nw_get_reg_status(simid, QOSA_NULL, &ps_status);
        QLOGI("ret=0x%x,ps_status=%d", ret, ps_status);
        if (QOSA_NW_ATTACHED(ps_status))
        {
            QLOGI("registration success");
        }
        else
        {
            goto wait_attach;
        }
        // Get current RAT
        current_rat = qosa_nw_get_current_rat(simid);
        QLOGI("current_rat:%d", current_rat);

        // Get current network operator name
        ret = qosa_nw_get_oper_name(simid, &long_oper_name, &short_oper_name);
        QLOGI(
            "ret=0x%x, long_oper_name:%s,length=%d, short_oper_name:%s,length=%d",
            ret,
            long_oper_name.name,
            long_oper_name.length,
            short_oper_name.name,
            short_oper_name.length
        );

        // Get current serving cell info
        ret = qosa_nw_get_scell_info(simid, &scell_info);
        if (ret != QOSA_NW_ERR_OK)
        {
            QLOGI("ret=0x%x", ret);
        }
        else
        {
            if (current_rat == QOSA_NW_RAT_4G)
            {
                QLOGI("serving cell info output");

                QLOGI("mcc=%d, mnc=%d, mnc digit num=%d", scell_info.lte.plmn.mcc, scell_info.lte.plmn.mnc, scell_info.lte.plmn.mnc_digit_num);

                QLOGI(
                    "act=%d, cellid=%x, tac=%x, pcid=%d, earfcn=%d, band=%d, is_tdd=%d",
                    scell_info.act,
                    scell_info.lte.cellid,
                    scell_info.lte.tac,
                    scell_info.lte.pcid,
                    scell_info.lte.earfcn,
                    scell_info.lte.band,
                    scell_info.lte.is_tdd
                );

                QLOGI("ul_bandwidth=%d, dl_bandwidth=%d, srxlev=%d, squal=%d", scell_info.lte.ul_bandwidth, scell_info.lte.dl_bandwidth, scell_info.lte.srxlev);

                QLOGI("rssi=%d, rsrp=%d, rsrq=%d, sinr=%d", scell_info.lte.rssi, scell_info.lte.rsrp, scell_info.lte.rsrq, scell_info.lte.sinr);

                //get csq rssi example
                qosa_uint8_t csq_rssi = 0;

                csq_rssi = nw_demo_lte_get_csq_rssi(scell_info.lte.rssi);

                QLOGI("example csq_rssi %d", csq_rssi);

            }
        }
        QLOGI("====================nw demo end===================");
    }
}

/**
 * @brief network demo initialization function
 *
 * This function is used to initialize network demo functionality, create network demo task
 *
 */
void unir_network_demo_init(void)
{
    int err = 0;
    // Create network demo task
    err = qosa_task_create(&g_nw_task, 1024 * 4, QOSA_PRIORITY_NORMAL, "QNWDEMO", nw_demo_task, QOSA_NULL);
    if (err != QOSA_OK)
    {
        QLOGD("nw demo task create error");
        return;
    }
}
