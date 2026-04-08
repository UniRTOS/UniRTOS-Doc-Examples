/*****************************************************************/ /**
* @file uart_demo.h
* @brief
* @author bronson.zhan@quectel.com
* @date 2025-04-23
*
* @copyright Copyright (c) 2023 Quectel Wireless Solution, Co., Ltd.
* All Rights Reserved. Quectel Wireless Solution Proprietary and Confidential.
*
* @par EDIT HISTORY FOR MODULE
* <table>
* <tr><th>Date <th>Version <th>Author <th>Description"
* <tr><td>2025-04-23 <td>1.0 <td>Bronson.Zhan <td> Init
* </table>
**********************************************************************/
#ifndef __UART_DEMO_H__
#define __UART_DEMO_H__

#include "qosa_def.h"
#include "qosa_sys.h"

/*===========================================================================
 *  Macro Definition
 ===========================================================================*/

#define CONFIG_QUECOS_UART_DEMO_TASK_STACK_SIZE 4096
#define QUEC_UART_DEMO_TASK_PRIO                QOSA_PRIORITY_NORMAL
#define QUEC_TEST_UART_PORT                     QOSA_UART_PORT_1

/*===========================================================================
  *  Enum
  ===========================================================================*/

/**
  * @enum  qosa_uart_demo_case_e
  * @brief uart test case.
  */
typedef enum
{
    QOSA_UART_DEMO_OUTPUT = 0,       /*!< UART output test: continuous printing test */
    QOSA_UART_DEMO_READ_1,           /*!< UART read test 1: read as much as received */
    QOSA_UART_DEMO_READ_2,           /*!< UART read test 2: read 1024 bytes every 5 seconds, other caching (can be used to test flow control) */
    QOSA_UART_DEMO_BAUDRATE,         /*!< UART baud rate switching test: test switching between various baud rates */
    QOSA_UART_DEMO_CHANGE_CCIO_MODE, /*!< CCIO mode switching test: AT mode for 20s, user UART mode for 20s, print prompts during switching (this test is only for EIGEN platform, no effect on other platforms) */
    QOSA_UART_DEMO_MAX,
} qosa_uart_demo_case_e;

void quec_demo_uart_case_switch(qosa_uart_demo_case_e caseNo);
void quec_uart_demo_init(void);

#endif /* __UART_DEMO_H__ */