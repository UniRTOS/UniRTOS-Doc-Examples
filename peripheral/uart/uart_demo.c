/*****************************************************************/ /**
* @file uart_demo.c
* @brief
* @author bronson.zhan@quectel.com
* @date 2025-04-23
*
* @copyright Copyright (c) 2023 Quectel Wireless Solution, Co., Ltd.
* All Rights Reserved. Quectel Wireless Solution Proprietary and Confidential.
*
* @par EDIT HISTORY FOR MODULE
* <table>
* <tr><th>Date <th>Version <th>Author <th>Description
* <tr><td>2025-04-23 <td>1.0 <td>Bronson.Zhan <td> Init
* </table>
**********************************************************************/
#include "qosa_sys.h"
#include "qosa_uart.h"
#include "qosa_def.h"
#include "qosa_log.h"

#include "uart_demo.h"

/*===========================================================================
 *  Macro Definition
 ===========================================================================*/

#define QOS_LOG_TAG                         LOG_TAG_UART_API

/*===========================================================================
 *  Variate
 ===========================================================================*/

static qosa_task_t  g_quec_uart_demo_task = QOSA_NULL;
static qosa_uint8_t g_uart_data[1024] = {0};

static qosa_uint16_t g_uart_test_case = QOSA_UART_DEMO_OUTPUT;

/*===========================================================================
 *  Static API Functions
 ===========================================================================*/
 /**
 * @brief UART事件回调处理函数
 * 
 * 该函数用于处理UART端口的各种事件指示，包括接收数据、发送完成和发送缓冲区低水位等事件。
 * 当事件发生时，会将事件信息通过UART发送出去。
 * 
 * @param cb_param UART回调参数结构体指针，包含端口号、事件ID和用户数据等信息
 */
static void quec_uart_ind(qosa_uart_cb_param_t *cb_param)
{
    qosa_uart_port_number_e port = cb_param->port;
    qosa_uint32_t           event_id = cb_param->event_id;
    char                    data[128] = {0};
    qosa_snprintf(data, sizeof(data), "port=%d, event_id=%d, user_data=%s", port, event_id, (unsigned char *)cb_param->user_data);
    // 根据不同的UART事件类型进行相应处理
    if (cb_param->event_id & QOSA_UART_EVENT_RX_INDICATE)
    {
        qosa_uart_write(port, (unsigned char *)data, sizeof(data));
    }
    else if (cb_param->event_id & QOSA_UART_EVENT_TX_COMPLETE)
    {
        qosa_uart_write(port, (unsigned char *)data, sizeof(data));
    }
    else if (cb_param->event_id & QOSA_UART_EVENT_TX_LOW)
    {
        qosa_uart_write(port, (unsigned char *)data, sizeof(data));
    }
}

/**
 * @brief UART功能处理函数，用于配置和测试UART接口的各种功能。
 *
 * 该函数初始化UART端口，注册回调函数，并根据全局变量 g_uart_test_case 的值执行不同的测试用例，
 * 包括输出数据、读取数据、波特率切换以及模式切换等操作。
 *
 */
 static void quec_uart_demo_process(void *ctx)
{
    int ret = 0;

    qosa_uart_status_monitor_t monitor = {0};
    monitor.callback = quec_uart_ind; /* 注册回调函数 */
    monitor.event_mask = QOSA_UART_EVENT_RX_INDICATE | QOSA_UART_EVENT_TX_COMPLETE;
    monitor.user_data = "Hello, Uart!";
    
    /* 注册UART事件回调 */
    qosa_uart_register_cb(QUEC_TEST_UART_PORT, &monitor);

    /* 配置UART通信参数：波特率、数据位、停止位、校验位、流控 */
    qosa_uart_config_t dcb_config = {0};
    dcb_config.baudrate = QOSA_UART_BAUD_115200;
    dcb_config.data_bit = QOSA_UART_DATABIT_8;
    dcb_config.flow_ctrl = QOSA_FC_NONE;
    dcb_config.parity_bit = QOSA_UART_PARITY_NONE;
    dcb_config.stop_bit = QOSA_UART_STOP_1;

    qosa_uart_ioctl(QUEC_TEST_UART_PORT, QOSA_UART_IOCTL_SET_DCB_CFG, (void *)&dcb_config);
    
        /* 打开UART端口 */
    qosa_uart_open(QUEC_TEST_UART_PORT);

    while (1)
    {
        switch (g_uart_test_case)
        {
            /* 测试UART发送功能 */
            case QOSA_UART_DEMO_OUTPUT: {
                qosa_task_sleep_sec(1);
                qosa_uart_write(QUEC_TEST_UART_PORT, (unsigned char *)"hello Quectel\r\n", 15);
            }
            break;
            /* 测试UART接收功能（通过回调处理） */
            case QOSA_UART_DEMO_READ_1: {
                qosa_task_sleep_sec(1);
                /* Received data in uart callback */
            }
            break;
            /* 测试UART接收功能（主动读取） */
            case QOSA_UART_DEMO_READ_2: {
                qosa_task_sleep_sec(5);
                qosa_uart_read(QUEC_TEST_UART_PORT, (unsigned char *)&g_uart_data, 1024);

                QLOGI("recv uart data %s", g_uart_data);
                ret = qosa_uart_write(QUEC_TEST_UART_PORT, (unsigned char *)&g_uart_data, 1024);
                QLOGI("qosa_uart_write ret = %d", ret);
             }
            break;
            /* 测试不同波特率下的UART通信 */
            case QOSA_UART_DEMO_BAUDRATE: {
                const qosa_uint32_t baudRateList[] = {0, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};

                int i;
                for (i = 0; i < sizeof(baudRateList) / sizeof(baudRateList[0]); i++)
                {
                    qosa_uart_ioctl(QUEC_TEST_UART_PORT, QOSA_UART_IOCTL_CHANGE_BAUDRATE, (void *)&baudRateList[i]);
                    qosa_task_sleep_sec(1);
                    qosa_uart_write(QUEC_TEST_UART_PORT, (unsigned char *)"Baudrate TEST\r\n", 15);
                    qosa_task_sleep_sec(1);
                }
            }
            break;
            /* 测试UART模式切换功能（Uart模式与AT命令模式） */
            case QOSA_UART_DEMO_CHANGE_CCIO_MODE: {
                qosa_uart_mode_e ccio_mode;
                int              i;
                ccio_mode = QOSA_UART_MODE_NORMAL;
                qosa_uart_ioctl(QUEC_TEST_UART_PORT, QOSA_UART_IOCTL_SET_CCIO_MODE, (void *)&ccio_mode);
                qosa_uart_write(QUEC_TEST_UART_PORT, (unsigned char *)"Enter Uart Mode\r\n", 17);
                for (i = 0; i < 20; i++)
                {
                    qosa_uart_write(QUEC_TEST_UART_PORT, (unsigned char *)"Waiting...\r\n", 12);
                    qosa_task_sleep_sec(1);
                }
                
                ccio_mode = QOSA_UART_MODE_AT;
                qosa_uart_write(QUEC_TEST_UART_PORT, (unsigned char *)"Enter AT Mode\r\n", 15);
                qosa_task_sleep_sec(1); /* 等待发送结束 */
                qosa_uart_ioctl(QUEC_TEST_UART_PORT, QOSA_UART_IOCTL_SET_CCIO_MODE, (void *)&ccio_mode);
                qosa_task_sleep_sec(20);
            }
            break;
            default:
                break;
        }
    }
}

/*===========================================================================
 *  Public API Functions
 ===========================================================================*/

void quec_demo_uart_case_switch(qosa_uart_demo_case_e caseNo)
{
    g_uart_test_case = caseNo;
}

void quec_uart_demo_init(void)
{
    QLOGI("enter Quectel UART DEMO !!!");
    if (g_quec_uart_demo_task == QOSA_NULL)
    {
        qosa_task_create(
            &g_quec_uart_demo_task,
            CONFIG_QUECOS_UART_DEMO_TASK_STACK_SIZE,
            QUEC_UART_DEMO_TASK_PRIO,
            "uart_demo",
            quec_uart_demo_process,
            QOSA_NULL,
            1
        );
    }
}