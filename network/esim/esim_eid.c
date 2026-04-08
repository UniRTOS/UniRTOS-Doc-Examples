char eid[ESIM_EID_STR_LEN+1] = {0};
esim_result_errno_e ret = ESIM_INVALID_PARAM_ERR;

osa_memset(eid, 0x00, ESIM_EID_STR_LEN+1);
ret = esim_intf_eid_get(eid);
utils_printf("eid:%s", eid);