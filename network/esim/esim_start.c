esim_result_errno_e ret = ESIM_INVALID_PARAM_ERR;
char iccid[] = "89320420000111550651";

ret = esim_intf_profile_handle(ESIM_PROFILE_OPT_ENABLE, iccid, osa_strlen(iccid), OSA_TRUE);

utils_printf("iccid[%s] enable ret:%x", iccid, ret);