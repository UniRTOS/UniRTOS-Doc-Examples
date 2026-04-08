/* 默认参数初始化宏 */
#define QOSA_SMS_PARAM_DEFAULT_INIT { \
    .simid= 0,                             /*!< 默认SIM的id为0 */ \
    .format = QOSA_EASY_SMS_TEXT,          /*!< 默认TEXT短信 */\
    .coding = QOSA_EASY_SMS_CODING_UCS2,   /*!< 默认UCS2编码 */\
    .storage = {QOSA_SMS_STOR_ME, QOSA_SMS_STOR_ME, QOSA_SMS_STOR_ME} /*!< 默认配置存储器1:ME  存储器2:ME  存储器3:ME */\
    .smsc = QOSA_NULL,                     /*!< 默认无短信中心 */\
    .delivery_report = QOSA_TRUE,          /*!< 默认不配置短信回执 */\
    .vp = 0xFF,                            /*!< 默认短信有效期最大值 */\
    .smc_retry_cnt = 0,                    /*!< 默认SMC层不重传 */\
    .smr_retry_cnt= 0,                     /*!< 默认SMR层不重传 */\
    .ignore_cscs = QOSA_FALSE,             /*!< 默认不忽略CSCS配置 */\
    .reserved = {0}                        /*!< 默认保留值全为0 */\
}