/*****************************************************************/ /**
* @file lcd_demo.c
* @brief LCD demonstration application implementation
* @author bronson.zhan@quectel.com
* @date 2025-11-18
*
* @copyright Copyright (c) 2023 Quectel Wireless Solution, Co., Ltd.
* All Rights Reserved. Quectel Wireless Solution Proprietary and Confidential.
*
* @par EDIT HISTORY FOR MODULE
* <table>
* <tr><th>Date <th>Version <th>Author <th>Description
* <tr><td>2025-11-18 <td>1.0 <td>Bronson.Zhan <td> Initial version
* </table>
**********************************************************************/

#include "qosa_sys.h"
#include "qosa_lcd.h"
#include "qosa_gpio.h"
#include "qosa_rtc.h"
#include "qosa_def.h"
#include "qosa_log.h"
#include "lcd_demo.h"
#include "logo.h"

/*===========================================================================
 * Macro Definition
 ===========================================================================*/

/** @brief Log tag for LCD API module */
#define QOS_LOG_TAG                            LOG_TAG_LCD_API

/*===========================================================================
 * Global Variables
 ===========================================================================*/

/** @brief LCD demonstration task handle */
static qosa_task_t   g_quec_lcd_demo_task = QOSA_NULL;

/** @brief Current active LCD test case */
static qosa_uint8_t  g_lcd_test_case = QOSA_LCD_DEMO_API_TEST;

/*===========================================================================
 * External API Functions / Variables
 ===========================================================================*/

/*===========================================================================
 * Static Function Implementations
 ===========================================================================*/

/**
 * @brief Perform LCD refresh rate test
 * 
 * This function measures the screen refresh rate by continuously
 * updating the display with moving graphics and counting the number
 * of frames rendered within a fixed time period.
 * 
 * @note The test runs for 5 seconds and calculates average FPS
 */
static void quec_refresh_rate_test(void) 
{
    QLOGI("Beginning refresh rate test...");
    
    qosa_uint32_t frame_count = 0, current_time = 0, fps = 0, elapsed = 0, pos = 0;
    
    // Get start time using system tick counter
    qosa_uint32_t start_time = qosa_get_system_tick_cnt(), end_time = 0, total_time = 0;
    
    // Run test for 5 seconds
    while ((qosa_get_system_tick_cnt() - start_time) < 5000) 
    {
        // Clear screen with white background
        qosa_lcd_clear_screen(QUEC_LCD_PORT, WHITE);
    
        // Display Quectel logo at moving position
        qosa_lcd_write(QUEC_LCD_PORT, (qosa_uint8_t *)g_QUECTEL_214x36, 
                      pos % 26, pos % 283, 
                      (pos % 26) + 214 - 1, 
                      (pos % 283) + 36 - 1);
        
        pos += 10; // Increment position for animation effect
        
        frame_count++;
        
        // Calculate real-time frame rate (update every 60 frames)
        if (frame_count % 60 == 0) 
        {
            current_time = qosa_get_system_tick_cnt();
            elapsed = current_time - start_time;
            fps = (frame_count * 1000.0f) / elapsed;
        }
    }
    
    // Calculate final frame rate
    end_time = qosa_get_system_tick_cnt();
    total_time = end_time - start_time;
    fps = (frame_count * 1000.0f) / total_time;
    
    QLOGI("Test Results: frame_count: %lu, total_time: %lu ms, refresh_rate: %d FPS", 
           frame_count, total_time, fps);
}

/**
 * @brief Main LCD demonstration process
 * 
 * This function implements the main demonstration logic including:
 * - Hardware initialization
 * - LCD controller configuration
 * - Test case execution based on current selection
 * 
 * @param ctx Context pointer (unused in this implementation)
 */
static void quec_lcd_demo_process(void *ctx)
{
    qosa_uint32_t ret = 0;
    qosa_uint32_t lcd_id = 0;
    qosa_lcd_drv_cfg_t *lcd_cfg = QOSA_NULL;

    // Delay 3 seconds to allow other system components to initialize
    qosa_task_sleep_ms(3000);

    /* Configure GPIO pins for 4-Wire SPI mode */
    qosa_pin_set_func(QUEC_LCD_RST_PIN, QUEC_LCD_RST_FUNC);  // LCD Reset pin
    qosa_pin_set_func(QUEC_LCD_CS_PIN, QUEC_LCD_CS_FUNC);    // LCD Chip Select pin
    qosa_pin_set_func(QUEC_LCD_DS_PIN, QUEC_LCD_DS_FUNC);    // LCD Data/Command Select pin
    qosa_pin_set_func(QUEC_LCD_DOUT_PIN, QUEC_LCD_DOUT_FUNC);// LCD Data Output pin
    qosa_pin_set_func(QUEC_LCD_CLK_PIN, QUEC_LCD_CLK_FUNC);  // LCD Clock pin

    /* Configure LCD driver chip settings */
    qosa_lcd_ioctl(QUEC_LCD_PORT, QOSA_LCD_IOCTL_SET_DRV_CFG, (void *)QUEC_LCD_DRV_CFG_ADDR);

    QLOGI("Initializing LCD controller...");
    ret = qosa_lcd_init(QUEC_LCD_PORT);
    if (ret != QOSA_LCD_SUCCESS)
    {
        QLOGE("LCD initialization failed with error code: %d", ret);
        return;
    }

    QLOGV("LCD demonstration test sequence starting...");
    
    // Main demonstration loop
    while (1)
    {
        qosa_task_sleep_ms(1000);
        
        switch (g_lcd_test_case)
        {
            case QOSA_LCD_DEMO_API_TEST:
            {
                // Comprehensive API functionality test
                QLOGI("Testing LCD deinitialization...");
                ret = qosa_lcd_deinit(QUEC_LCD_PORT);
                if (ret != QOSA_LCD_SUCCESS)
                {
                    QLOGE("LCD deinitialization failed with error code: %d", ret);
                }

                QLOGI("Testing LCD reinitialization...");
                ret = qosa_lcd_init(QUEC_LCD_PORT);
                if (ret != QOSA_LCD_SUCCESS)
                {
                    QLOGE("LCD reinitialization failed with error code: %d", ret);
                }

                qosa_task_sleep_ms(1000);
                QLOGI("Filling screen with RED color");
                qosa_lcd_clear_screen(QUEC_LCD_PORT, RED);

                qosa_task_sleep_ms(1000);
                QLOGI("Displaying Quectel logo");
                qosa_lcd_write(QUEC_LCD_PORT, (qosa_uint8_t *)g_QUECTEL_214x36, 
                              0, 0, 214 - 1, 36 - 1);

                qosa_task_sleep_ms(1000);
                QLOGI("Turning display OFF");
                qosa_lcd_display_off(QUEC_LCD_PORT);

                QLOGI("Attempting to display BLUE (should not be visible)");
                qosa_lcd_clear_screen(QUEC_LCD_PORT, BLUE);

                qosa_task_sleep_ms(1000);
                QLOGI("Turning display ON");
                qosa_lcd_display_on(QUEC_LCD_PORT);

                QLOGI("Filling screen with GREEN color");
                qosa_lcd_clear_screen(QUEC_LCD_PORT, GREEN);
            }
            break;
            
            case QOSA_LCD_DEMO_READ_ID:
            {
                // Read LCD controller ID test
                lcd_cfg = QUEC_LCD_DRV_CFG_ADDR;
                lcd_id = lcd_cfg->operation->readId(QUEC_LCD_PORT);
                QLOGI("LCD Controller ID: 0x%08x", lcd_id);
            }
            break;
            
            case QOSA_LCD_DEMO_SHOW_PICTURE:
            {
                // Color and logo display test
                qosa_task_sleep_ms(1000);
                QLOGI("Filling screen with RED color");
                qosa_lcd_clear_screen(QUEC_LCD_PORT, RED);
        
                qosa_task_sleep_ms(1000);
                QLOGI("Filling screen with GREEN color");
                qosa_lcd_clear_screen(QUEC_LCD_PORT, GREEN);
        
                qosa_task_sleep_ms(1000);
                QLOGI("Filling screen with BLUE color");
                qosa_lcd_clear_screen(QUEC_LCD_PORT, BLUE);
        
                qosa_task_sleep_ms(1000);
                QLOGI("Displaying Quectel logo");
                qosa_lcd_write(QUEC_LCD_PORT, (qosa_uint8_t *)g_QUECTEL_214x36, 
                              0, 0, 214 - 1, 36 - 1);
            }
            break;
            
            case QOSA_LCD_DEMO_FLESH_RATE:
            {
                // Screen refresh rate measurement test
                QLOGI("Starting screen refresh rate measurement");
                quec_refresh_rate_test();
            }
            break;
            
            default:
                QLOGW("Unknown test case: %d", g_lcd_test_case);
            break;
        }
    }
}

/*===========================================================================
 * Public API Functions
 ===========================================================================*/

/**
 * @brief Switch between different LCD test cases
 * 
 * This function allows dynamic switching between various LCD demonstration
 * test cases during runtime.
 * 
 * @param caseNo The test case number to activate
 * 
 * @return None
 */
void quec_demo_spi_case_switch(qosa_lcd_demo_case_e caseNo)
{
    // Update current active test case
    g_lcd_test_case = caseNo;
    QLOGI("LCD test case switched to: %d", caseNo);
}

/**
 * @brief Initialize and start LCD demonstration application
 * 
 * This function creates the LCD demonstration task which will handle
 * all LCD operations and test sequences. The task runs with normal
 * priority and has its own stack space.
 * 
 * @return None
 */
void quec_lcd_demo_init(void)
{
    QLOGI("Initializing Quectel LCD Demonstration Application");
    
    if (g_quec_lcd_demo_task == QOSA_NULL)
    {
        /* Create LCD demonstration task */
        qosa_task_create(
            &g_quec_lcd_demo_task,
            CONFIG_QUECOS_LCD_DEMO_TASK_STACK_SIZE,
            QUEC_LCD_DEMO_TASK_PRIO,
            "lcd_demo",
            quec_lcd_demo_process,
            QOSA_NULL,
            1
        );
        
        QLOGI("LCD demonstration task created successfully");
    }
    else
    {
        QLOGW("LCD demonstration task already exists");
    }
}