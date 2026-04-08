char ver[64] = {0};
int len = 0;

osa_memset(ver, 0x00, 64);
len = esim_intf_ver_get(ver, 64);
utils_printf("esim ver:%s", ver);