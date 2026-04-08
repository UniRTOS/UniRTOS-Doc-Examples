#define utils_printf(...) OSA_LOG_V(LOG_TAG_ESIM, ##__VA_ARGS__)


osa_eercode_common_e ret = OSA_OK;
ret  = esim_intf_server_start();
utils_printf("create at process task:%x", ret);