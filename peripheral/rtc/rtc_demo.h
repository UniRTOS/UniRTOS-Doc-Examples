/*****************************************************************/ /**
* @file qosa_rtc.h
* @brief RTC operation interface
* @author larson.li@quectel.com
* @date 2023-08-15
*
* @copyright Copyright (c) 2023 Quectel Wireless Solution, Co., Ltd.
* All Rights Reserved. Quectel Wireless Solution Proprietary and Confidential.
*
* @par EDIT HISTORY FOR MODULE
* <table>
* <tr><th>Date <th>Version <th>Author <th>Description"
* <tr><td>2023-08-15 <td>1.0 <td>Larson.Li <td> Init
* </table>
**********************************************************************/
#ifndef __QOSA_RTC_H__
#define __QOSA_RTC_H__

#include "qosa_def.h"
#include "qosa_sys.h"

#include <time.h>

/*===========================================================================
 * Enum
===========================================================================*/
#define TIMEZONE_QUARTER_MIN (-48)
#define TIMEZONE_QUARTER_MAX (56)

// clang-format off

/**
 * @enum  qosa_rtc_enable_e
 * @brief RTC enable configuration enumeration
 */
typedef enum
{
    QOSA_RTC_CFG_DISABLE = 0,
    QOSA_RTC_CFG_ENABLE  = 1,
} qosa_rtc_enable_e;

/**
 * @enum  qosa_rtc_tz_e
 * @brief RTC timezone configuration after network registration
 */
typedef enum
{
    QOSA_RTC_CFG_TZ_UPDATE       = 0,    /* 0: After network registration, update to network timezone */
    QOSA_RTC_CFG_TZ_NO_UPDATE    = 1,    /* 1: After network registration, maintain original timezone */
    QOSA_RTC_CFG_TZ_RESET_ZERO   = 2,    /* 2: After network registration, reset timezone to 0 */
} qosa_rtc_tz_e;

/**
 * @enum qosa_rtc_error_e
 * @brief RTC error codes
 *
 */
typedef enum
{
    QOSA_RTC_ERR_OK = 0,

    QOSA_RTC_ERR_INVALID_PARAM   = 1 | QOSA_COMPONENT_API_RTC,   /*!< Invalid parameter */
    QOSA_RTC_ERR_SET_PARAM,                                      /*!< Configuration error */
    QOSA_RTC_ERR_GET_PARAM,                                      /*!< Read error */

} qosa_rtc_error_e;
// clang-format on

/*===========================================================================
 *  Struct
===========================================================================*/
/**
 * @struct qosa_time_t
 * @brief Define universal time structure
 */
typedef struct
{
    qosa_uint64_t seconds;      /*!< Seconds */
    qosa_uint64_t microseconds; /*!< Microseconds */
} qosa_time_info_t;

/**
 * @struct qosa_timezone_t
 * @brief Define timezone information structure
 */
typedef struct
{
    int tz_minuteswest; /*!< minutes west of Greenwich */
    int tz_dsttime;     /*!< type of dst correction */
} qosa_timezone_t;

/**
 * @struct qosa_rtc_time_t
 * @brief Define RTC time structure, used to represent broken-down time (year, month, day, hour, minute, second, etc.)
 */
typedef struct
{
    int tm_sec;   // seconds [0,59]
    int tm_min;   // minutes [0,59]
    int tm_hour;  // hour [0,23]
    int tm_mday;  // day of month [1,31]
    int tm_mon;   // month of year [0,11],0=January,11=December
    int tm_year;  // year starting from 1900, 2024 → 124 (i.e. tm_year + 1900 = 2024)
    int tm_wday;  // wday [0-6], sunday = 0,6=SAT
} qosa_rtc_time_t;

/**
 * @struct qosa_rtc_cfg_t
 * @brief Define RTC configuration structure
 */
typedef struct
{
    qosa_rtc_enable_e nv_cfg;  /* Whether to read nv saved time at boot as initial rtc value (ENABLE: read, DISABLE not read, default not read) */
    qosa_rtc_enable_e rtc_cfg; /* Whether to read rtc register time at boot as initial rtc value, if VBAT power is disconnected, time will reset to 2000-1-1 (ENABLE: read, DISABLE not read, default not read) */
    qosa_rtc_enable_e nwt_cfg; /* Whether to synchronize base station time to rtc time after connecting to base station (ENABLE: synchronize, DISABLE not synchronize, default synchronize) */
    qosa_rtc_tz_e     tz_cfg;  /* Timezone configuration after network registration, default is 0 */
} qosa_rtc_cfg_t;

/*===========================================================================
 * Function
===========================================================================*/
/**
 * @brief Get internal system tick count, not time
 * @note Tick cycle: (1000 / CONFIG_QOSA_SYS_TICK_PER_SECOND)
 *       Calculate time corresponding to specified tick: tick * (1000 / CONFIG_QOSA_SYS_TICK_PER_SECOND)
 *
 * @return qosa_uint32_t
 *       - Return system internal clock tick count
 */
qosa_uint32_t qosa_get_system_tick_cnt(void);

/**
 * @brief RTC callback function, used to receive alarm notification
 * @param None
 * @return None
 */
typedef void (*qosa_rtc_cb_ptr)(void);

/**
 * @brief Get system time interface function
 *
 * @param[out] qosa_time_t * time
 *           - Return current system time structure
 *
 * @retuen qosa_bool_t
 *       - Success returns OSA_TRUE, get system time successfully
 *       - Failure returns OSA_FALSE
 *
 */
qosa_bool_t qosa_get_system_time(qosa_time_info_t *time);

/**
 * @brief Get system time seconds interface
 *
 * @return qosa_time_t
 *       - Return seconds calculated from 1970.1.1
 */
qosa_time_t qosa_get_system_time_seconds(void);

/**
 * @brief Get system time milliseconds interface
 *
 * @return qosa_time_t
 *    - Return milliseconds calculated from 1970.1.1
 */
qosa_time_t qosa_get_system_time_milliseconds(void);

/**
 * @brief Get system time microseconds interface
 *
 * @return qosa_time_t
 *       - Return microseconds calculated from 1970.1.1
 */
qosa_time_t qosa_get_system_time_microseconds(void);

/**
 * @brief Used for user to set system RTC time
 *
 * @param[in] qosa_time_t time
 *          - Unix time address to be set (UTC time)
 *
 * @return int
 *       - Success returns 0,
 *       - Failure returns -1
 */
int qosa_rtc_set_time(qosa_time_t time);

/**
 * @brief Used for user to get system RTC time
 *
 * @param[out] qosa_time_t * time
 *           - Unix format time to get (UTC time)
 *
 * @return int
 *       - Success returns 0
 *       - Failure returns -1
 */int qosa_rtc_get_time(qosa_time_t *time);

/**
 * @brief Used for user to get local time
 *
 * @param[out] qosa_time_t * time
 *           - Local time to get, UTC time + timezone
 *
 * @return int
 *       - Success returns 0
 *       - Failure returns -1
 */
int qosa_rtc_get_localtime(qosa_time_t *time);

/**
 * @brief Get system timezone
 *
 * @return int
 *       - Return system timezone
 */
int qosa_rtc_get_timezone(void);
/**
 * @brief Set system timezone
 * @param[in] int time_zone  -48 ~ 56
 *           - Timezone
 * @return int
 *       - Success returns 0
 *       - Failure returns -1
 */
int qosa_rtc_set_timezone(int time_zone);

/**
 * @brief Convert UTC timestamp of type qosa_time_t to broken-down time,
 *        and store the result in user-provided qosa_rtc_time_t structure
 *
 * @param[in] timer
 *            Pointer to UTC timestamp (seconds since 1970-01-01 00:00:00 UTC)
 *
 * @param[out] result
 *            Pointer to structure to save converted broken-down time (similar to struct tm)
 *
 * @return
 *        - Success: return result pointer
 *        - Failure: return NULL
 */
 qosa_rtc_time_t *qosa_rtc_gmtime_r(const qosa_time_t *timer, qosa_rtc_time_t *result);

/**
 * @brief Convert struct tm type to timestamp (usually seconds since 1970-01-01 00:00:00 UTC) to Greenwich Mean
 *  Time (GMT, i.e. UTC
 *
 * @param[in] qosa_rtc_time_t * rtc_time
 *            - Structure storing time
 *
 * @param[out] const qosa_time_t *timer
 *            - Store UTC time
 *
 * @return int
 *        - Success returns 0
 *        - Failure returns others
 */
int qosa_rtc_mktime(qosa_rtc_time_t *rtc_time, qosa_time_t *timer);

/**
 * @brief Set rtc alarm time
 *
 * @param[in] qosa_rtc_time_t * rtc_time
 *            - Structure storing time
 * @return qosa_rtc_error_e
 *       - Success returns 0
 *       - Failure returns others
 */
 qosa_rtc_error_e qosa_rtc_set_alarm(qosa_rtc_time_t *rtc_time);

/**
 * @brief Get rtc alarm time
 *
 * @param[in] qosa_rtc_time_t * rtc_time
 *            - Structure storing time
 * @return qosa_rtc_error_e
 *       - Success returns QOSA_RTC_ERR_OK
 *       - Failure returns others
 */
qosa_rtc_error_e qosa_rtc_get_alarm(qosa_rtc_time_t *rtc_time);

/**
 * @brief Turn on or off rtc alarm
 *
 * @param[in] qosa_uint8_t on_off
 *            - rtc alarm function switch, 0: off 1: on
 * @return qosa_rtc_error_e
 *       - Success returns QOSA_RTC_ERR_OK
 *       - Failure returns others
 */
 qosa_rtc_error_e qosa_rtc_enable_alarm(qosa_uint8_t on_off);

/**
 * @brief Register rtc alarm callback function
 *
 * @param[in] qosa_rtc_cb_ptr cb
 *            - alarm callback function
 * @return qosa_rtc_error_e
 *       - Success returns QOSA_RTC_ERR_OK
 *       - Failure returns others
 */
qosa_rtc_error_e qosa_rtc_register_cb(qosa_rtc_cb_ptr cb);

/**
 * @brief Configure rtc
 *
 * @param[in] qosa_rtc_cfg_t * rtc_time
 *            - Structure storing configuration
 * @return qosa_rtc_error_e
 *       - Success returns QOSA_RTC_ERR_OK
 *       - Failure returns others
 */
 qosa_rtc_error_e qosa_rtc_set_cfg(qosa_rtc_cfg_t *cfg);

/**
 * @brief Get rtc configuration
 *
 * @param[in] qosa_rtc_cfg_t * rtc_time
 *            - Structure storing configuration
 * @return qosa_rtc_error_e
 *       - Success returns QOSA_RTC_ERR_OK
 *       - Failure returns others
 */
qosa_rtc_error_e qosa_rtc_get_cfg(qosa_rtc_cfg_t *cfg);

/**
 * @brief Print rtc time
 *
 * @param[in] qosa_rtc_time_t * rtc_time
 *            - Structure storing time
 * @return qosa_rtc_error_e
 *       - Success returns QOSA_RTC_ERR_OK
 *       - Failure returns others
 */
qosa_rtc_error_e qosa_rtc_print_time(qosa_rtc_time_t *rtc_time);

#endif /* __QOSA_RTC_H__ */
