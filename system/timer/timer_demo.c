/*****************************************************************/ /**
* @file at_demo_cmd.c
* @brief
* @author amara.wang@om
* @date 2025-10-13
*
* @copyright Copyright (c) 2025 Quectel Wireless Solution, Co., Ltd.
* All Rights Reserved. Quectel Wireless Solution Proprietary and Confidential.
*
**********************************************************************/
#include "qosa_log.h"
#include "qosa_def.h"
#include "qosa_sys.h"

#define QOS_LOG_TAG LOG_TAG

/*********************
*
* UniRTOS advises against using CMSIS (Cortex Microcontroller Software Interface Standard) interfaces
*They strongly suggest using their own QOSA (UniRTOS Open System Architecture) system interfaces instead
*
*********************/

// Timer callback function
static void timer_callback_function(void *argument)
{
    int *count = (int *)argument;
    (*count)++;
    QLOGV("Timer triggered! Count: %d\n", *count);
}

// One-shot timer test
static int32_t test_one_shot_timer(void)
{
    qosa_timer_t one_shot_timer = QOSA_NULL;
    int          timer_count = 0;

    QLOGV("\n1. Test one-shot timer\n");

    if (qosa_timer_create(&one_shot_timer, timer_callback_function, &timer_count) != QOSA_OK)
    {
        QLOGE("Failed to create one-shot timer\n");
        return QOSA_FALSE;
    }
    QLOGV("One-shot timer created successfully\n");

    // Start one-shot timer (triggers after 100ms)
    qosa_timer_start(one_shot_timer, 100, QOSA_FALSE);
    QLOGV("One-shot timer started\n");
    QLOGV("Timer running status: %d\n", qosa_timer_is_running(one_shot_timer));

    // Wait for timer to trigger
    qosa_task_sleep_ms(150);
    QLOGV("Final count: %d\n", timer_count);

    // Delete timer
    qosa_timer_delete(one_shot_timer);
    QLOGV("One-shot timer deleted\n");

    return QOSA_TRUE;
}

// Periodic timer test
static int32_t test_periodic_timer(void)
{
    qosa_timer_t periodic_timer = QOSA_NULL;
    int          periodic_count = 0;
    int          i;

    QLOGV("\n2. Test periodic timer\n");

    if (qosa_timer_create(&periodic_timer, timer_callback_function, &periodic_count) != QOSA_OK)
    {
        QLOGV("Failed to create periodic timer\n");
        return QOSA_FALSE;
    }
    QLOGV("Periodic timer created successfully\n");

    // Start periodic timer (triggers every 50ms)
    qosa_timer_start(periodic_timer, 50, QOSA_TRUE);
    QLOGV("Periodic timer started\n");

    // Let timer trigger several times
    for (i = 0; i < 3; i++)
    {
        qosa_task_sleep_ms(60);
        QLOGV("Check %d - Count: %d, Running status: %d\n", i + 1, periodic_count, qosa_timer_is_running(periodic_timer));
    }

    // Test stop functionality
    QLOGV("Stop timer\n");
    qosa_timer_stop(periodic_timer);
    QLOGV("Status after stop: %d\n", qosa_timer_is_running(periodic_timer));

    // Restart test
    QLOGV("Restart timer\n");
    qosa_timer_start(periodic_timer, 50, QOSA_TRUE);
    qosa_task_sleep_ms(60);
    QLOGV("Count after restart: %d\n", periodic_count);

    // Cleanup
    qosa_timer_delete(periodic_timer);
    QLOGV("Periodic timer deleted\n");

    return QOSA_TRUE;
}

// Error cases test
static int32_t test_error_cases(void)
{
    qosa_timer_t invalid_timer = QOSA_NULL;
    QLOGV("\n3. Error cases test\n");
    QLOGV("Null pointer creation: %s\n", qosa_timer_create(&invalid_timer, QOSA_NULL, QOSA_NULL) == QOSA_OK ? "Fail" : "Pass");
    if (invalid_timer != QOSA_NULL)
    {
        qosa_timer_delete(invalid_timer);
    }
    QLOGV("Invalid timer start: %d\n", qosa_timer_start(QOSA_NULL, 100, QOSA_FALSE));
    QLOGV("Invalid timer stop: %d\n", qosa_timer_stop(QOSA_NULL));
    QLOGV("Invalid timer status: %d\n", qosa_timer_is_running(QOSA_NULL));
    QLOGV("Invalid timer delete: %d\n", qosa_timer_delete(QOSA_NULL));

    return QOSA_TRUE;
}

// Main test function
int32_t osTimerTest_init(void)
{
    QLOGV("Timer Function Test Start\n");

    // Execute tests
    if (test_one_shot_timer() != QOSA_TRUE)
    {
        QLOGE("One-shot timer test failed\n");
        return QOSA_FALSE;
    }

    if (test_periodic_timer() != QOSA_TRUE)
    {
        QLOGE("Periodic timer test failed\n");
        return QOSA_FALSE;
    }

    if (test_error_cases() != QOSA_TRUE)
    {
        QLOGE("Error cases test failed\n");
        return QOSA_FALSE;
    }

    QLOGV("\nTimer test completed\n");
    return QOSA_TRUE;
}
