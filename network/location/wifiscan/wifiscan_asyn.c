void wifiscan_async_callback(void *user_data, qosa_wifiscan_error_e result, qosa_uint32_t ap_cnt, qosa_wifi_ap_info_t *ap_infos)
{
    QOSA_UNUSED(user_data);
    
    // 检查扫描结果是否成功
    if ((QOSA_WIFISCAN_SUCCESS == result) && (QOSA_NULL != ap_infos))
    {
        // 输出扫描结果
        wifiscan_log("Scanned %d APs", ap_cnt);
        
        for (qosa_uint32_t i = 0; i < ap_cnt; i++)
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
}

void wifiscan_async_example(void)
{
    qosa_wifiscan_config_t wifiscan_config = {0};
    
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
        return;
    }
    
    // 注册异步扫描回调函数
    if (QOSA_WIFISCAN_SUCCESS != qosa_wifiscan_register_cb(wifiscan_async_callback, QOSA_NULL))
    {
        return;
    }
    
    // 打开 WiFi 扫描设备
    if (QOSA_WIFISCAN_SUCCESS != qosa_wifiscan_open())
    {
        return;
    }
    
    // 启动异步扫描
    qosa_wifiscan_async();
    
    // 注意：扫描结果将通过回调函数返回，此处不需要等待
}