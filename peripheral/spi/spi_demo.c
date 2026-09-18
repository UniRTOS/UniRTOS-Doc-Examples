/*****************************************************************/ /**
* @file spi_demo.c
* @brief
* @author bronson.zhan@quectel.com
* @date 2025-05-15
*
* @copyright Copyright (c) 2023 Quectel Wireless Solution, Co., Ltd.
* All Rights Reserved. Quectel Wireless Solution Proprietary and Confidential.
*
* @par EDIT HISTORY FOR MODULE
* <table>
* <tr><th>Date <th>Version <th>Author <th>Description
* <tr><td>2025-05-08 <td>1.0 <td>Bronson.Zhan <td> Init
* </table>
**********************************************************************/
#include "qosa_sys.h"
#include "qosa_spi.h"
#include "qosa_gpio.h"
#include "qosa_def.h"
#include "qosa_log.h"
#include <string.h>
#include "spi_demo.h"

/*===========================================================================
 *  Macro Definition
 ===========================================================================*/
#define QOS_LOG_TAG   LOG_TAG_DEMO

/*===========================================================================
 *  Variate
 ===========================================================================*/
static qosa_task_t   g_quec_spi_demo_task = QOSA_NULL;
static qosa_uint16_t g_spi_test_case = QOSA_SPI_DEMO_LOOPBACK;
static qosa_uint16_t g_spi_is_init = 0;

/*===========================================================================
 *  Static API Functions
 ===========================================================================*/
/**
 * @brief SPI Demo function
 *
 * This function is used to demonstrate various test cases of SPI interface, including loopback test, clock rate switching,
 * polarity phase configuration, NSS mode switching, bit order switching, data width switching, large data packet sending, etc.
 * The function will select different test branches to execute based on the value of global variable g_spi_test_case.
 *
 * @param ctx Task context pointer (unused)
 *
 */
 static void quec_spi_demo_process(void *ctx)
{
    qosa_uint8_t data_w[32] = {0};
    qosa_uint8_t data_r[32] = {0};

    qosa_task_sleep_ms(3000);

    // Configure SPI pin functions CS CLK MOSO MISO
    qosa_pin_set_func(QUEC_EC800ZCNLC_SPI1_MOSI_PIN, QUEC_EC800ZCNLC_SPI1_MOSI_PIN_FUNC);
    qosa_pin_set_func(QUEC_EC800ZCNLC_SPI1_MISO_PIN, QUEC_EC800ZCNLC_SPI1_MISO_PIN_FUNC);
    qosa_pin_set_func(QUEC_EC800ZCNLC_SPI1_CLK_PIN, QUEC_EC800ZCNLC_SPI1_CLK_PIN_FUNC);
    qosa_pin_set_func(QUEC_EC800ZCNLC_SPI1_CS_PIN, QUEC_EC800ZCNLC_SPI1_HARDWARE_CS_PIN_FUNC);
    
    QLOGV("SPI INIT !!!");
    // Initialize SPI interface
    if (qosa_spi_init(QUEC_TEST_SPI_PORT, QOSA_SPI_TRANSMIT_POLLING, QOSA_SPI_CLK_13MHZ) != QOSA_SPI_SUCCESS)
    {
        QLOGE("SPI INIT FAILED");
    }
    
    g_spi_is_init = 1;
    QLOGV("SPI DEMO TESTING...");
    while (1)
    {
        qosa_task_sleep_ms(1000);
        switch (g_spi_test_case)
        {
            case QOSA_SPI_DEMO_LOOPBACK:                                     // Loopback test (MOSI/MISO short circuit test)
            {
                qosa_memset(data_w, (qosa_uint8_t)qosa_rand(), 1);                     // Randomly fill send data
                qosa_spi_write_read(QUEC_TEST_SPI_PORT, data_r, data_w, 1);  // Execute SPI read-write operation
                QLOGD("[SPI LOOPBACK DEMO]: try send (0x%x)", data_w[0]);
                QLOGD("[SPI LOOPBACK DEMO]: try receive (0x%x)", data_r[0]);
            }
            break;
            case QOSA_SPI_DEMO_MODE: {
                // TODO
            }
            break;
            case QOSA_SPI_DEMO_CLK:  // Rate switching test
            {
                int            i;
                const qosa_uint32_t clkList[]
                    = {QOSA_SPI_CLK_812_5KHZ,
                       QOSA_SPI_CLK_1_625MHZ,
                       QOSA_SPI_CLK_3_25MHZ,
                       QOSA_SPI_CLK_6_5MHZ,
                       QOSA_SPI_CLK_13MHZ,
                       QOSA_SPI_CLK_26MHZ,
                       QOSA_SPI_CLK_52MHZ};

                QLOGV("[SPI CLK DEMO DEMO]: try deinit spi");
                // Deinitialize spi
                if (qosa_spi_deinit(QUEC_TEST_SPI_PORT) != QOSA_SPI_SUCCESS)
                {
                    QLOGE("SPI DEINIT FAILED");
                }
                QLOGV("[SPI CLK DEMO DEMO]: try init spi");
                // Use DMA mode test, observe data sending performance at different rates
                if (qosa_spi_init(QUEC_TEST_SPI_PORT, QOSA_SPI_TRANSMIT_DMA, QOSA_SPI_CLK_13MHZ) != QOSA_SPI_SUCCESS)
                {
                    QLOGE("SPI INIT FAILED");
                }
                // Send data at different clocks
                for (i = 0; i < sizeof(clkList) / sizeof(clkList[0]); i++)
                {
                    QLOGD("[SPI CLK DEMO]: try change clock (%d)", clkList[i]);
                    qosa_spi_ioctl(QUEC_TEST_SPI_PORT, QOSA_SPI_IOCTL_SET_CLOCK_FREQUENCY, (void *)&clkList[i]);
                    qosa_task_sleep_sec(1);

                    qosa_memset(data_w, 0x66, 10);
                    qosa_spi_write(QUEC_TEST_SPI_PORT, data_w, 10);

                    qosa_task_sleep_sec(1);
                }
            }
            break;
            case QOSA_SPI_DEMO_CLK_POLARITY_PHASE:  // Polarity phase test
            {
                int            i;
                const qosa_uint32_t frameFormatList[] = {QOSA_SPI_CLK_CPOL0_CPHA0, QOSA_SPI_CLK_CPOL0_CPHA1, QOSA_SPI_CLK_CPOL1_CPHA0, QOSA_SPI_CLK_CPOL1_CPHA1};

                // Different clock format tests
                for (i = 0; i < sizeof(frameFormatList) / sizeof(frameFormatList[0]); i++)
                {
                    QLOGD("[SPI CLK POLARITY PHASE DEMO]: try change clock polarity and phace (%d)", frameFormatList[i]);
                    qosa_spi_ioctl(QUEC_TEST_SPI_PORT, QOSA_SPI_IOCTL_SET_CLK_POLARITY_PHASE, (void *)&frameFormatList[i]);
                    qosa_task_sleep_sec(1);

                    qosa_memset(data_w, 0x66, 1);
                    qosa_spi_write(QUEC_TEST_SPI_PORT, data_w, 1);

                    qosa_task_sleep_sec(1);
                }
            break;
            case QOSA_SPI_DEMO_NSS_MODE:  // CS mode switching test
            {
                int            i;
                const qosa_uint32_t nssModeList[] = {QOSA_SPI_NSS_MASTER_HARDWARE, QOSA_SPI_NSS_MASTER_SOFTWARE};  // todo test: QOSA_SPI_NSS_SLAVE_HARDWARE

                for (i = 0; i < sizeof(nssModeList) / sizeof(nssModeList[0]); i++)
                {
                    QLOGD("[SPI NSS MODE DEMO]: try change nss mode (%d)", nssModeList[i]);
                    qosa_spi_ioctl(QUEC_TEST_SPI_PORT, QOSA_SPI_IOCTL_SET_NSS_MODE, (void *)&nssModeList[i]);
                    qosa_task_sleep_sec(1);
                    
                    if (i == QOSA_SPI_NSS_MASTER_HARDWARE)  // CS pin controlled by hardware
                    {
                        qosa_pin_set_func(QUEC_EC800ZCNLC_SPI1_CS_PIN, QUEC_EC800ZCNLC_SPI1_HARDWARE_CS_PIN_FUNC);
                        qosa_memset(data_w, 0x77, 1);
                        qosa_spi_write(QUEC_TEST_SPI_PORT, data_w, 1);
                    }
                    else if (i == QOSA_SPI_NSS_MASTER_SOFTWARE)
                    {
                        // CS pin controlled by software, configure CS as GPIO
                        qosa_pin_set_func(QUEC_EC800ZCNLC_SPI1_CS_PIN, QUEC_EC800ZCNLC_SPI1_CS_PIN_FUNC);
                        qosa_gpio_init(QUEC_EC800ZCNLC_SPI1_CS_GPIO_NUM, QOSA_GPIO_DIRECTION_OUTPUT, QOSA_GPIO_PULL_UP, QOSA_GPIO_LEVEL_HIGH);

                        qosa_memset(data_w, 0x88, 1);
                        // Pull CS pin high and low through software before and after writing data
                        qosa_gpio_set_level(QUEC_EC800ZCNLC_SPI1_CS_GPIO_NUM, QOSA_GPIO_LEVEL_LOW);
                        qosa_spi_write(QUEC_TEST_SPI_PORT, data_w, 1);
                        qosa_gpio_set_level(QUEC_EC800ZCNLC_SPI1_CS_GPIO_NUM, QOSA_GPIO_LEVEL_HIGH);
                    }
                    else if (i == QOSA_SPI_NSS_SLAVE_HARDWARE)
                    {
                        // TODO
                    }

                    qosa_task_sleep_sec(1);
                }
            }
            break;
            case QOSA_SPI_DEMO_BIT_ORDER:  // Bit order switching test
            {
                int            i;
                const qosa_uint32_t bitOrderList[] = {QOSA_SPI_MSB_FIRST, QOSA_SPI_LSB_FIRST};

                // Different bit order send data test
                for (i = 0; i < sizeof(bitOrderList) / sizeof(bitOrderList[0]); i++)
                {
                    QLOGD("[SPI BIT ORDER DEMO]: try change bit order (%d)", bitOrderList[i]);
                    qosa_spi_ioctl(QUEC_TEST_SPI_PORT, QOSA_SPI_IOCTL_SET_BIT_ORDER, (void *)&bitOrderList[i]);
                    qosa_task_sleep_sec(1);

                    qosa_memset(data_w, 0x99, 1);
                    qosa_spi_write(QUEC_TEST_SPI_PORT, data_w, 1);

                    qosa_task_sleep_sec(1);
                }
            }
            break;
            case QOSA_SPI_DEMO_DATA_WIDTH:  // Bit width switching test
            {
                int            i;
                const qosa_uint32_t dataWidthList[] = {QOSA_SPI_WIDTH_1_BYTES, QOSA_SPI_WIDTH_2_BYTES};

                // Different bit width send data test
                for (i = 0; i < sizeof(dataWidthList) / sizeof(dataWidthList[0]); i++)
                {
                    QLOGD("[SPI DATA WIDTH DEMO]: try change data width (%d)", dataWidthList[i]);
                    qosa_spi_ioctl(QUEC_TEST_SPI_PORT, QOSA_SPI_IOCTL_SET_DATA_WIDTH, (void *)&dataWidthList[i]);
                    qosa_task_sleep_sec(1);

                    qosa_memset(data_w, 0x11, 16);
                    qosa_spi_write(QUEC_TEST_SPI_PORT, data_w, 16);

                    qosa_task_sleep_sec(1);
                }
            }
            break;
            case QOSA_SPI_DEMO_BIG_PACKAGE:  // Large data send test
            {
                int            i;
                const qosa_uint32_t sendMode[] = {QOSA_SPI_TRANSMIT_POLLING, QOSA_SPI_TRANSMIT_DMA};

                // Polling and DMA mode send different size data test
                for (i = 0; i < sizeof(sendMode) / sizeof(sendMode[0]); i++)
                {
                    QLOGD("[SPI BIT PACKAGE DEMO]: try change spi mode (%d)", sendMode[i]);

                    QLOGV("[SPI BIT PACKAGE DEMO]: try deinit spi");
                    if (qosa_spi_deinit(QUEC_TEST_SPI_PORT) != QOSA_SPI_SUCCESS)
                    {
                        QLOGE("SPI DEINIT FAILED");
                    }
                    QLOGV("[SPI BIT PACKAGE DEMO]: try init spi");
                    if (qosa_spi_init(QUEC_TEST_SPI_PORT, sendMode[i], QOSA_SPI_CLK_13MHZ) != QOSA_SPI_SUCCESS)
                    {
                        QLOGE("SPI INIT FAILED");
                    }

                    qosa_uint8_t *bigPackageData;

                    QLOGV("[SPI BIT PACKAGE DEMO]: try malloc 5bytes data");
                    bigPackageData = (qosa_uint8_t *)qosa_malloc(5);

                    // 5-byte data send test
                    if (bigPackageData != QOSA_NULL)
                    {
                        qosa_memset(bigPackageData, 0x12, 5);
                        qosa_spi_write(QUEC_TEST_SPI_PORT, bigPackageData, 5);

                        qosa_free(bigPackageData);
                        bigPackageData = QOSA_NULL;
                    }
                    else
                    {
                        QLOGE("[SPI BIT PACKAGE DEMO]: malloc 5bytes failed");
                    }
                    
                    QLOGV("[SPI BIT PACKAGE DEMO]: try malloc 5*1024bytes data");
                    bigPackageData = (qosa_uint8_t *)qosa_malloc(5 * 1024);
                    // 5K-byte data send test
                    if (bigPackageData != QOSA_NULL)
                    {
                        qosa_memset(bigPackageData, 0x12, 5 * 1024);
                        qosa_spi_write(QUEC_TEST_SPI_PORT, bigPackageData, 5 * 1024);

                        qosa_free(bigPackageData);
                        bigPackageData = QOSA_NULL;
                    }
                    else
                    {
                        QLOGE("[SPI BIT PACKAGE DEMO]: malloc 5*1024bytes failed");
                    }
                    
                    QLOGV("[SPI BIT PACKAGE DEMO]: try malloc 10*1024bytes data");
                    bigPackageData = (qosa_uint8_t *)qosa_malloc(10 * 1024);
                    // 10k-byte data send test
                    if (bigPackageData != QOSA_NULL)
                    {
                        qosa_memset(bigPackageData, 0x12, 10 * 1024);
                        qosa_spi_write(QUEC_TEST_SPI_PORT, bigPackageData, 10 * 1024);

                        qosa_free(bigPackageData);
                        bigPackageData = QOSA_NULL;
                    }
                    else
                    {
                        QLOGE("[SPI BIT PACKAGE DEMO]: malloc 10*1024bytes failed");
                    }
                }
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
/**
 * @brief Switch SPI test case
 *
 * @param caseNo Test case number to switch to
 *
 * @return No return value
 *
 * This function is used to switch SPI interface test cases. If SPI is already initialized,
 * configure NSS mode as hardware master mode and set corresponding pin functions.
 */
 void quec_demo_spi_case_switch(qosa_spi_demo_case_e caseNo)
{
    // If SPI is already initialized, configure NSS mode and pin functions
    if (g_spi_is_init == 1)
    {
        qosa_uint8_t nssmode = QOSA_SPI_NSS_MASTER_HARDWARE;
        // Set SPI1 CS pin as hardware CS function
        qosa_pin_set_func(QUEC_EC800ZCNLC_SPI1_CS_PIN, QUEC_EC800ZCNLC_SPI1_HARDWARE_CS_PIN_FUNC);
        // Set SPI NSS mode as master hardware mode
        qosa_spi_ioctl(QUEC_TEST_SPI_PORT, QOSA_SPI_IOCTL_SET_NSS_MODE, (void *)&nssmode);
    }
    // Update current test case number
    g_spi_test_case = caseNo;
}

void quec_spi_demo_init(void)
{
    QLOGV("enter Quectel spi DEMO !!!");
    if (g_quec_spi_demo_task == QOSA_NULL)
    {
        qosa_task_create(
            &g_quec_spi_demo_task,
            CONFIG_QUECOS_SPI_DEMO_TASK_STACK_SIZE,
            QUEC_SPI_DEMO_TASK_PRIO,
            "spi_demo",
            quec_spi_demo_process,
            QOSA_NULL,
            1
        );
    }
}