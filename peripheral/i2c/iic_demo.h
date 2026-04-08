/*****************************************************************/ /**
* @file iic_demo.h
* @brief
* @author bronson.zhan@quectel.com
* @date 2025-05-08
*
* @copyright Copyright (c) 2023 Quectel Wireless Solution, Co., Ltd.
* All Rights Reserved. Quectel Wireless Solution Proprietary and Confidential.
*
* @par EDIT HISTORY FOR MODULE
* <table>
* <tr><th>Date <th>Version <th>Author <th>Description"
* <tr><td>2025-05-08 <td>1.0 <td>Bronson.Zhan <td> Init
* </table>
**********************************************************************/
#ifndef __IIC_DEMO_H__
#define __IIC_DEMO_H__

#include "qosa_def.h"
#include "qosa_sys.h"

/*===========================================================================
 *  Macro Definition
 ===========================================================================*/
#define CONFIG_QUECOS_IIC_DEMO_TASK_STACK_SIZE 4096                  // Demo task stack size configuration
#define QUEC_IIC_DEMO_TASK_PRIO                QOSA_PRIORITY_NORMAL  // Demo task priority configuration
#define QUEC_IIC_DEMO_CHANNEL                  0                     // IIC channel configuration

#define QUEC_IIC_DEMO_SLAVE_ADDR               0x1B                  // Slave address (using 5616 as slave for testing)
#define QUEC_IIC_DEMO_REG_02                   0x02                  // 5616 register

#define QUEC_IIC_DEMO_WRITE_VALUE              0x12                  // IIC write test data

// EC800ZCNLC IIC0 pin configuration
#define QUEC_EC800ZCNLC_I2C0_SCL_PIN           67
#define QUEC_EC800ZCNLC_I2C0_SCL_FUNC          (2)

#define QUEC_EC800ZCNLC_I2C0_SDA_PIN           66
#define QUEC_EC800ZCNLC_I2C0_SDA_FUNC          (2)

/*===========================================================================
 *  Function Declaration
 ===========================================================================*/
void quec_iic_demo_init(void);

#endif /* __IIC_DEMO_H__ */