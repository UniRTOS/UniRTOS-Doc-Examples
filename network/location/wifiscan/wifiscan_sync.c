#include "qosa_def.h"
#include "qosa_log.h"
#include "qosa_wifiscan.h"

void wifiscan_sync_example(void)
{
    qosa_uint16_t ap_cnt = 0;
    qosa_wifi_ap_info_t *ap_infos = QOSA_NULL;
    qosa_wifiscan_config_t wifiscan_config = {0};
    
    // 分配 AP 信息内存空间
    ap_infos = (qosa_wifi_ap_info_t *)qosa_calloc(QOSA_WIFI_SCAN_DEFAULT_AP_CNT, sizeof(qosa_wifi_ap_info_t));
    if (ap_infos == QOSA_NULL)
    {
        return;
    }
    
    // 配置 WiFi 扫描参数
    wifiscan_config.max_ap_cnt = QOSA_WIFI_SCAN_DEFAULT_AP_CNT;
    wifiscan_config.scan_round = QOSA_WIFI_SCAN_DEFAULT_ROUND;
    wifiscan_config.channel = QOSA_WIFISCAN_CHANNEL_ALL_BIT;
    wifiscan_config.ch_time = QOSA_WIFI_SCAN_DEFAULT_TIME;
    wifiscan_config.max_timeout = QOSA_WIFI_SCAN_TOTAL_TIME_VAL_DEF;
    wifiscan_config.scan_timeout = QOSA_WIFI_SCAN_EACH_ROUND_TIMEOUT_VAL_DEF;
    wifiscan_config.wifi_priority = QOSA_WIFI_SCAN_PRIORITY_VAL_DEF;
    
    // 设置扫描配置
    if (QOSA_WIFISCAN_SUCCESS != qosa_wifiscan_option_set(&wifiscan_config))
    {
        qosa_free(ap_infos);
        return;
    }
    
    // 打开 WiFi 扫描设备
    if (QOSA_WIFISCAN_SUCCESS != qosa_wifiscan_open())
    {
        qosa_free(ap_infos);
        return;
    }
    
    // 执行同步扫描
    if (QOSA_WIFISCAN_SUCCESS == qosa_wifiscan_do(&ap_cnt, ap_infos))
    {
        // 输出扫描结果
        wifiscan_log("Scanned %d APs", ap_cnt);
        
        for (qosa_uint16_t i = 0; i < ap_cnt; i++)
        {
            wifiscan_log("AP[%d]: SSID=%s, BSSID=%02X:%02X:%02X:%02X:%02X:%02X, RSSI=%ddBm, CH=%d",
                i,
                ap_infos[i].ssid,
                ap_infos[i].bssid[0], ap_infos[i].bssid[1], ap_infos[i].bssid[2],
                ap_infos[i].bssid[3], ap_infos[i].bssid[4], ap_infos[i].bssid[5],
                ap_infos[i].rssival,
                ap_infos[i].channel);
        }
    }
    
    // 关闭 WiFi 扫描设备
    qosa_wifiscan_close();
    
    // 释放内存
    qosa_free(ap_infos);
}