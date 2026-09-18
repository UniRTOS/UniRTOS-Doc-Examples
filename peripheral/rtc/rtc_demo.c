/*****************************************************************/ /**
* @file rtc_demo.c
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
#include "qosa_sys.h"
#include "qosa_rtc.h"
#include "qosa_def.h"
#include "qosa_log.h"

/*===========================================================================
 *  Macro Definition
 ===========================================================================*/
#define QOS_LOG_TAG   LOG_TAG_DEMO
#define CONFIG_QUECOS_RTC_DEMO_TASK_STACK_SIZE 4096

/*===========================================================================
 *  Variate
 ===========================================================================*/
static qosa_task_t g_quec_rtc_demo_task = QOSA_NULL;

/*===========================================================================
 *  Static API Functions
 ===========================================================================*/

/**
 * @brief RTC test callback function
 * @details This function serves as the callback handler when RTC alarm triggers, used to get and print alarm time information
 * @param None
 * @return None
 */
 void quec_rtc_test_callback(void)
{
    QLOGV("rtc test callback come in!");

    // Define and initialize RTC time structure
    qosa_rtc_time_t tm = {0};

    // Get and print alarm trigger time
    QLOGV("=========callback print alarm time===========\r\n");
    qosa_rtc_get_alarm(&tm);
    qosa_rtc_print_time(&tm);
}

/**
 * @brief RTC function Demo function
 *
 * This function demonstrates RTC (Real-Time Clock) related functions, including getting system time, setting timezone,
 * setting and getting time, local time conversion, timestamp and date mutual conversion, alarm setting, and RTC configuration reading.
 *
 * @param ctx Task context pointer, reserved parameter, not used
 *
 * @return No return value
 */
 static void quec_rtc_demo_process(void *ctx)
{
    int              ret = 0;
    qosa_time_info_t time_info = {0};
    qosa_time_t      seconds = 0;       //s
    qosa_time_t      milliseconds = 0;  //ms
    qosa_time_t      microseconds = 0;  //us
    qosa_time_t      unix_time = 0;
    qosa_time_t      set_time = 0;
    qosa_time_t      local_time = 0;
    int              time_zone = 0;
    qosa_rtc_time_t  tm = {0};
    qosa_uint8_t     on_off = 1;
    qosa_rtc_cfg_t   rtc_cfg = {0};

    // Delay 5s to prevent log loss
    qosa_task_sleep_sec(5);

    // Get system time and print
    qosa_get_system_time(&time_info);
    QLOGD("sec=%d,%d", time_info.seconds, time_info.microseconds);

    // Get seconds, milliseconds, microseconds time separately
    seconds = qosa_get_system_time_seconds();
    milliseconds = qosa_get_system_time_milliseconds();
    microseconds = qosa_get_system_time_microseconds();
    QLOGD("seconds=%d,%d,%d", seconds, milliseconds, microseconds);

    // Set timezone and time (UTC+8)
    time_zone = 32;
    qosa_rtc_set_timezone(time_zone);
    // Set time to Unix timestamp corresponding to May 7, 2025 08:00:00
    set_time = 1746576000;
    ret = qosa_rtc_set_time(set_time);
    if (ret != 0)
    {
        QLOGE("ret=%x", ret);
    }

    // Get current timezone and time
    time_zone = qosa_rtc_get_timezone();
    ret = qosa_rtc_get_time(&unix_time);
    if (ret != 0)
    {
        QLOGE("ret=%x", ret);
    }
    QLOGD("time_zone=%d,unix=%d", time_zone, unix_time);
     // Get local time
    ret = qosa_rtc_get_localtime(&local_time);
    QLOGD("ret=%d,local_time=%d", ret, local_time);

    // Convert between timestamp and date format
    qosa_rtc_gmtime_r(&unix_time, &tm);
    QLOGD("%d-%d-%d:%d:%d:%d", tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    QLOGD("wday=%d", tm.tm_wday);

    // Alarm function test - set alarm to trigger 30 seconds after current time
    local_time += 30;
    qosa_rtc_gmtime_r(&local_time, &tm);
    ret = qosa_rtc_set_alarm(&tm);
    QLOGD("ret=%d", ret);
    ret = qosa_rtc_get_alarm(&tm);
    QLOGD("ret=%d", ret);
    QLOGD("%d-%d-%d:%d:%d:%d", tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

    // Register alarm callback function and enable alarm
    qosa_rtc_register_cb(quec_rtc_test_callback);
    qosa_rtc_enable_alarm(on_off);
     // Get RTC configuration
    ret = qosa_rtc_get_cfg(&rtc_cfg);
    QLOGD("nv_cfg=%d,%d,%d,%d", rtc_cfg.nv_cfg, rtc_cfg.nwt_cfg, rtc_cfg.rtc_cfg, rtc_cfg.tz_cfg);
}

/*===========================================================================
 *  Public API Functions
 ===========================================================================*/

void quec_rtc_demo_init(void)
{
    QLOGV("enter Quectel RTC DEMO !!!");
    if (g_quec_rtc_demo_task == QOSA_NULL)
    {
        qosa_task_create(&g_quec_rtc_demo_task, CONFIG_QUECOS_RTC_DEMO_TASK_STACK_SIZE, QOSA_PRIORITY_NORMAL, "rtc_demo", quec_rtc_demo_process, QOSA_NULL);
    }
}
