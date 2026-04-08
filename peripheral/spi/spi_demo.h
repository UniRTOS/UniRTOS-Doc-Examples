/*****************************************************************/ /**
* @file spi_demo.h
* @brief
* @author bronson.zhan@quectel.com
* @date 2025-05-15
*
* @copyright Copyright (c) 2023 Quectel Wireless Solution, Co., Ltd.
* All Rights Reserved. Quectel Wireless Solution Proprietary and Confidential.
*
* @par EDIT HISTORY FOR MODULE
* <table>
* <tr><th>Date <th>Version <th>Author <th>Description"
* <tr><td>2025-05-15 <td>1.0 <td>Bronson.Zhan <td> Init
* </table>
**********************************************************************/
#ifndef __SPI_EXAMPLES_H__
#define __SPI_EXAMPLES_H__

#include "qosa_def.h"
#include "qosa_sys.h"

/*===========================================================================
 *  Macro Definition
 ===========================================================================*/

#define CONFIG_QUECOS_SPI_DEMO_TASK_STACK_SIZE    4096                  // Demo task stack size configuration
#define QUEC_SPI_DEMO_TASK_PRIO                   QOSA_PRIORITY_NORMAL  // Demo task priority configuration

// Pin configuration based on hardware used SPI
#define QUEC_EC800ZCNLC_SPI1_MOSI_PIN             (50)
#define QUEC_EC800ZCNLC_SPI1_MOSI_PIN_FUNC        (1)
#define QUEC_EC800ZCNLC_SPI1_MISO_PIN             (51)
#define QUEC_EC800ZCNLC_SPI1_MISO_PIN_FUNC        (1)
#define QUEC_EC800ZCNLC_SPI1_CLK_PIN              (53)
#define QUEC_EC800ZCNLC_SPI1_CLK_PIN_FUNC         (1)

// Software CS control pin configuration
#define QUEC_EC800ZCNLC_SPI1_CS_GPIO_NUM          QOSA_GPIO_12
#define QUEC_EC800ZCNLC_SPI1_CS_PIN               (52)
#define QUEC_EC800ZCNLC_SPI1_CS_PIN_FUNC          (0)

// Hardware CS control pin configuration
#define QUEC_EC800ZCNLC_SPI1_HARDWARE_CS_PIN_FUNC (1)

// Used SPI interface configuration
#define QUEC_TEST_SPI_PORT                        QOSA_SPI_PORT1

/*===========================================================================
  *  Enum
  ===========================================================================*/
/**
  * @enum  qosa_spi_demo_case_e
  * @brief spi test case.
  */
typedef enum
{
    QOSA_SPI_DEMO_LOOPBACK = 0,       /*!< Loopback test (MOSI/MISO short circuit test) */
    QOSA_SPI_DEMO_MODE,               /*!< SPI mode switching test */
    QOSA_SPI_DEMO_CLK,                /*!< Rate switching test */
    QOSA_SPI_DEMO_CLK_POLARITY_PHASE, /*!< Polarity phase test */
    QOSA_SPI_DEMO_NSS_MODE,           /*!< CS mode switching test */
    QOSA_SPI_DEMO_BIT_ORDER,          /*!< Bit order switching test */
    QOSA_SPI_DEMO_DATA_WIDTH,         /*!< Bit width switching test */
    QOSA_SPI_DEMO_BIG_PACKAGE,        /*!< Large data send test */
    QOSA_SPI_DEMO_MAX,
} qosa_spi_demo_case_e;

/*===========================================================================
 *  Function Declaration
 ===========================================================================*/
void quec_demo_spi_case_switch(qosa_spi_demo_case_e caseNo);
void quec_spi_demo_init(void);

#endif /* __SPI_EXAMPLES_H__ */