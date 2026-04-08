#include "qosa_def.h"
#include "qosa_log.h"
#include "qosa_sys.h"
#include "qcm_gnss_api.h"
#include "qosa_datacall.h"
#include "qosa_system_utils.h"

#define QOS_LOG_TAG                       LOG_TAG
#define GNSS_DEMO_TASK_STACK_SIZE 4096
#define GNSS_DEMO_TASK_PRIO       QOSA_PRIORITY_NORMAL

static qosa_task_t g_gnss_app_task;

// Callback function for handling GNSS notifications
void qapp_gnss_notify_cb(qcm_gnss_notify_e notify, qcm_gnss_err_e error_code)
{
    QLOGV("notify: %d, error_code: %d", notify, error_code);
}

// Callback function for processing NMEA sentences from GNSS module
void qapp_gnss_nmea_cb(const char *nmea, int nmea_len)
{
    if (QOSA_NULL == nmea || 0 == nmea_len) // Check if input is valid
    {
        return; // Return early if invalid
    }

    QLOGV("nmea %s", nmea); // Log received NMEA sentence
}

// Main handler process for GNSS operations
static void gnss_handler_process(void *arg)
{
    qcm_gnss_err_e err = QCM_GNSS_ERR_OK; // Initialize error variable
    qcm_gnss_cfg_t gnss_cfg = {0}; // Initialize GNSS configuration structure
    qcm_gnss_location_t location = {0}; // Initialize location data structure
    char resp_str[512] = {0}; // Buffer to hold formatted response string
    int  resp_len = 0; // Length of current response string
    int ret = 0; // General purpose return value
    double float1 = 0; // Temporary floating point variable
    double float2 = 0; // Another temporary floating point variable
    int    inter1 = 0; // Integer part of coordinate conversion

    char float_str1[32] = {0}; // String buffer for latitude/longitude formatting

    err = qcm_gnss_init(qapp_gnss_notify_cb, qapp_gnss_nmea_cb); // Initialize GNSS with callbacks
    if (QCM_GNSS_ERR_OK != err) // Check initialization result
    {
        QLOGE("qcm_gnss_init error"); // Log error message
        return; // Exit if initialization failed
    }

    // Get default GNSS configuration values
    ret = qcm_gnss_get_cfg(&gnss_cfg);
    if (QCM_GNSS_ERR_OK != ret) // Check retrieval result
    {
        QLOGE("qcm_gnss_get_cfg error"); // Log error message
        return; // Exit if retrieval failed
    }

    // Configure multi-constellation mode to use GPS, BeiDou, Galileo, and GLONASS
    gnss_cfg.multi_constel = QCM_MULTI_GP_GB_GA_GL;

    ret = qcm_gnss_set_cfg(&gnss_cfg); // Apply updated configuration
    if (QCM_GNSS_ERR_OK != ret) // Check setting result
    {
        QLOGE("qcm_gnss_set_cfg error"); // Log error message
        return; // Exit if setting failed
    }

    // Start GNSS positioning
    qcm_gnss_start();

    while (1) // Infinite loop to continuously get location updates
    {
        qosa_memset(resp_str, 0x00, sizeof(resp_str));
        resp_len = 0;

        qosa_task_sleep_sec(1); // Sleep for one second between iterations

        qcm_gnss_get_location(&location); // Retrieve latest location information

        // Format time into HHMMSS format and append to response string
        resp_len += (qosa_uint16_t)qosa_snprintf(
            (char *)(resp_str + resp_len),
            sizeof(resp_str) - resp_len,
            "%02d%02d%02d.00",
            location.time.tm_hour,
            location.time.tm_min,
            location.time.tm_sec
        );

        // Convert latitude from decimal degrees to degrees and minutes format
        float1 = location.latitude;
        inter1 = (int)float1;
        float2 = float1 - (int)float1;
        float2 *= 60;

        QLOGD("latitude_cardinal: %c", location.latitude_cardinal); // Debug log cardinal direction

        // Append formatted latitude with cardinal direction (North/South)
        if (location.latitude_cardinal == 'N')
        {
            qosa_utils_double_to_str_with_precision(float2, (qosa_uint8_t *)float_str1, sizeof(float_str1) - 1, 4);
            resp_len += (qosa_uint16_t)qosa_snprintf((char *)(resp_str + resp_len), sizeof(resp_str) - resp_len, ",%02d%sN", inter1, float_str1);
        }
        else if (location.latitude_cardinal == 'S')
        {
            qosa_utils_double_to_str_with_precision(float2, (qosa_uint8_t *)float_str1, sizeof(float_str1) - 1, 4);
            resp_len += (qosa_uint16_t)qosa_snprintf((char *)(resp_str + resp_len), sizeof(resp_str) - resp_len, ",%02d%sS", inter1, float_str1);
        }

        // Convert longitude similarly to latitude
        float1 = location.longitude;
        inter1 = (int)float1;
        float2 = float1 - (int)float1;
        float2 *= 60;

        QLOGD("longitude_cardinal: %c", location.longitude_cardinal); // Debug log cardinal direction

        // Append formatted longitude with cardinal direction (East/West)
        if (location.longitude_cardinal == 'E')
        {
            qosa_utils_double_to_str_with_precision(float2, (qosa_uint8_t *)float_str1, sizeof(float_str1) - 1, 4);
            resp_len += (qosa_uint16_t)qosa_snprintf((char *)(resp_str + resp_len), sizeof(resp_str) - resp_len, ",%03d%sE", inter1, float_str1);
        }
        else if (location.longitude_cardinal == 'W')
        {
            qosa_utils_double_to_str_with_precision(float2, (qosa_uint8_t *)float_str1, sizeof(float_str1) - 1, 4);
            resp_len += (qosa_uint16_t)qosa_snprintf((char *)(resp_str + resp_len), sizeof(resp_str) - resp_len, ",%03d%sW", inter1, float_str1);
        }

        // Limit HDOP value to maximum 99.9
        if (location.hdop >= 99.9)
        {
            location.hdop = 99.9;
        }

        // Format horizontal dilution of precision (HDOP)
        qosa_utils_double_to_str_with_precision(location.hdop, (qosa_uint8_t *)float_str1, sizeof(float_str1) - 1, 1);
        resp_len += (qosa_uint16_t)qosa_snprintf((char *)(resp_str + resp_len), sizeof(resp_str) - resp_len, ",%s", float_str1);

        // Format altitude (in meters above mean sea level)
        qosa_utils_double_to_str_with_precision(location.altitude, (qosa_uint8_t *)float_str1, sizeof(float_str1) - 1, 1);
        resp_len += (qosa_uint16_t)qosa_snprintf((char *)(resp_str + resp_len), sizeof(resp_str) - resp_len, ",%s", float_str1);

        // Format navigation mode/fixed status
        resp_len += (qosa_uint16_t)qosa_snprintf((char *)(resp_str + resp_len), sizeof(resp_str) - resp_len, ",%d", location.navmode);

        // Format course over ground (COG) in degrees (ddd.mm)
        inter1 = location.course;
        float1 = location.course - (float)inter1;
        float1 *= 100;
        qosa_utils_double_to_str_with_precision(float1, (qosa_uint8_t *)float_str1, sizeof(float_str1) - 1, 0);
        resp_len += (qosa_uint16_t)qosa_snprintf((char *)(resp_str + resp_len), sizeof(resp_str) - resp_len, ",%03d.%s", inter1, float_str1);

        // Format speed over ground (km/h)
        qosa_utils_double_to_str_with_precision(location.speed, (qosa_uint8_t *)float_str1, sizeof(float_str1) - 1, 1);
        resp_len += (qosa_uint16_t)qosa_snprintf((char *)(resp_str + resp_len), sizeof(resp_str) - resp_len, ",%s", float_str1);

        // Format speed over ground (knots), converting from km/h to knots
        qosa_utils_double_to_str_with_precision(location.speed / 1.852, (qosa_uint8_t *)float_str1, sizeof(float_str1) - 1, 1);
        resp_len += (qosa_uint16_t)qosa_snprintf((char *)(resp_str + resp_len), sizeof(resp_str) - resp_len, ",%s", float_str1);

        // Format date in DDMMYY format
        resp_len += (qosa_uint16_t)qosa_snprintf(
            (char *)(resp_str + resp_len),
            sizeof(resp_str) - resp_len,
            ",%02d%02d%02d",
            location.time.tm_mday,
            location.time.tm_mon,
            location.time.tm_year
        );

        // Format number of tracked satellites
        resp_len += (qosa_uint16_t)qosa_snprintf((char *)(resp_str + resp_len), sizeof(resp_str) - resp_len, ",%02d", location.satellites_tracked);

        QLOGD("resp_str: %s", resp_str); // Log final formatted response string
    }

    qcm_gnss_stop(); // Stop GNSS operation
    qcm_gnss_deinit(); // Deinitialize GNSS resources
}


/**
 * @brief GNSS Demo init
 */
void unir_gnss_demo_init(void)
{
    int         err = 0;

    err = qosa_task_create(&g_gnss_app_task, GNSS_DEMO_TASK_STACK_SIZE, GNSS_DEMO_TASK_PRIO, "gnss_demo", gnss_handler_process, QOSA_NULL);
    if (err != QOSA_OK)
    {
        QLOGE("app_task task create error");
        return;
    }
}
