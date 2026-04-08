esim_result_errno_e ret = ESIM_INVALID_PARAM_ERR;
osa_uint32_t sec = 3600;
ret = esim_intf_poll_interval_set(&sec);
utils_printf("ipa poll ret: %x", ret);