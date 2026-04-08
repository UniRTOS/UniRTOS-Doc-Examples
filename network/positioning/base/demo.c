/** LBS basic information configuration */
static qcm_lbs_basic_info_t g_basic_info    = {.type = 1,        /*!< Location type */
       .encrypt = 1,     /*!< Encryption flag */
       .key_index = 1,   /*!< Key index */
       .pos_format = 1,  /*!< Position format */
       .loc_method = 4}; /*!< Location method */
/** LBS authentication information configuration */
static qcm_lbs_auth_info_t g_auth_info    = {.user_name = QCM_LBS_DEMO_USER_NAME, /*!< Username */
       .user_pwd = QCM_LBS_DEMO_USER_PWD,   /*!< User password */
       .token = QCM_LBS_DEMO_TOKEN,         /*!< Authentication token */
       .imei = QCM_LBS_DEMO_IMEI,           /*!< Device IMEI */
       .rand = 2346};                       /*!< Random number */
/** LBS cell information configuration */
static qcm_lbs_cell_info_t g_lbs_cell_info[]    = {{.radio = 3,           /*!< Radio access technology type */
        .mcc = 460,           /*!< Mobile country code */
        .mnc = 0,             /*!< Mobile network code */
        .lac_id = 0x550B,     /*!< Location area code */
        .cell_id = 0xF2D4A48, /*!< Cell ID */
        .signal = 0,          /*!< Signal strength */
        .tac = 3,             /*!< Tracking area code */
        .bcch = 0,            /*!< Broadcast control channel */
        .bsic = 0,            /*!< Base station identity code */
        .uarfcndl = 0,        /*!< UTRA absolute radio frequency channel number downlink */
        .psc = 0,             /*!< Primary scrambling code */
        .rsrq = 0,            /*!< Reference signal received quality */
        .pci = 0,             /*!< Physical cell ID */
        .earfcn = 0}};        /*!< E-UTRA absolute radio frequency channel number */
/* ... (网络注册和数据连接建立代码省略) ... */
qcm_lbs_option_t user_option;
qosa_memset(&user_option, 0x00, sizeof(qcm_lbs_option_t));
user_option.pdp_cid = profile_idx;
user_option.sim_id = 0;
user_option.req_timeout = 60;
user_option.basic_info = &g_basic_info;
user_option.auth_info = &g_auth_info;
user_option.cell_num = 1;
user_option.cell_info = &g_lbs_cell_info[0];
g_lbs_cli = qcm_lbs_client_new();
if (g_lbs_cli <= 0){
    QLOGV("lbs client create failed");
    goto exit;
}
if (QCM_LBS_SUCCESS == qcm_lbs_get_position(g_lbs_cli, "www.queclocator.com", &user_option, unir_lbs_result_cb, QOSA_NULL)){
    qosa_sem_wait(g_lbs_semp, QOSA_WAIT_FOREVER);
}else{
    QLOGV("lbs failed");
}