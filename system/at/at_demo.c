#include "qosa_sys.h"
#include "qosa_def.h"
#include "qosa_log.h"
#include "qosa_at_cmd.h"
#include "qosa_at_param.h"

/*===========================================================================
 * 宏定义与声明
 ===========================================================================*/
#define QOS_LOG_TAG "EXAMPLE_AT"

/* 声明执行函数 */
void unir_exec_example_qexamplehello_cmd(qosa_at_cmd_t *cmd);

/*===========================================================================
 * AT 命令描述表
 ===========================================================================*/
static const qosa_at_desc_t unir_examples_at_desc[] = {
    {"+QEXAMPLEHELLO", unir_exec_example_qexamplehello_cmd, 0},
};

/*===========================================================================
 * AT 命令处理函数
 ===========================================================================*/
/**
 * @brief 处理 AT+QEXAMPLEHELLO 命令
 * 支持类型:
 * AT+QEXAMPLEHELLO=? (TEST)
 * AT+QEXAMPLEHELLO?  (READ)
 * AT+QEXAMPLEHELLO   (EXE)
 * AT+QEXAMPLEHELLO="string",num (SET)
 */
void unir_exec_example_qexamplehello_cmd(qosa_at_cmd_t *cmd)
{
    char          resp[128] = {0};
    char         *test_str = QOSA_NULL;
    qosa_bool_t   paramok = QOSA_TRUE;
    qosa_uint32_t num = 0;

    switch (cmd->type)
    {
        case QOSA_AT_CMD_SET: { // 处理设置命令
            // 获取第一个字符串参数
            test_str = (char *)qosa_at_param_string(cmd->params[0], &paramok);
            if (!paramok) {
                qosa_at_resp_cme_error(cmd->dev_port, QOSA_ERR_AT_CME_PARAM_INVALID);
                return;
            }

            // 如果有第二个参数，获取整数
            if (cmd->param_count == 2) {
                num = qosa_at_param_uint(cmd->params[1], &paramok);
                if (!paramok) {
                    qosa_at_resp_cme_error(cmd->dev_port, QOSA_ERR_AT_CME_PARAM_INVALID);
                    return;
                }
            }

            // 返回设置的结果 
            qosa_snprintf(resp, sizeof(resp), "+QEXAMPLEHELLO: \"%s\",%d", test_str, num);
            qosa_at_resp_cmd(cmd->dev_port, QOSA_ATCI_RESULT_CODE_OK, QOSA_CMD_RC_OK, resp, 1);
        }
        break;

        case QOSA_AT_CMD_TEST: { // 处理测试命令 AT+QEXAMPLEHELLO=? 
            qosa_snprintf(resp, sizeof(resp), "%s", "+QEXAMPLEHELLO: \"test_str\",<num>");
            qosa_at_resp_cmd(cmd->dev_port, QOSA_ATCI_RESULT_CODE_OK, QOSA_CMD_RC_OK, resp, 1);
        }
        break;

        case QOSA_AT_CMD_READ: { // 处理查询命令 AT+QEXAMPLEHELLO?
            qosa_snprintf(resp, sizeof(resp), "%s", "+QEXAMPLEHELLO: Hello World!");
            qosa_at_resp_cmd(cmd->dev_port, QOSA_ATCI_RESULT_CODE_OK, QOSA_CMD_RC_OK, resp, 1);
        }
        break;

        case QOSA_AT_CMD_EXE: { // 处理执行命令 AT+QEXAMPLEHELLO 
            qosa_at_resp_cmd(cmd->dev_port, QOSA_ATCI_RESULT_CODE_OK, QOSA_CMD_RC_OK, QOSA_NULL, 1);
        }
        break;

        default:
            qosa_at_resp_cme_error(cmd->dev_port, QOSA_ERR_AT_CME_OPERATION_NOT_SUPPORTED);
        break;
    }
}

/*===========================================================================
 * 初始化函数
 ===========================================================================*/
void qexample_hello_init(void)
{
    // 向系统注册自定义 AT 命令表
    qosa_at_parser_add_cust_at(unir_examples_at_desc, QOSA_ARRAY_SIZE(unir_examples_at_desc));
    QLOGV("QEXAMPLEHELLO demo initialized.");
}