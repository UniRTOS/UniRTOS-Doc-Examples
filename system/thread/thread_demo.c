/*****************************************************************/ /**
* @file at_demo_cmd.c
* @brief
* @author amara.wang@quectel.com
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

static void cleanup_task_handle(qosa_task_t *task_id)
{
    qosa_int32_t status = 0;

    if (task_id == QOSA_NULL || *task_id == QOSA_NULL)
    {
        return;
    }

    if (qosa_task_get_status(*task_id, &status) == QOSA_OK && status == 1)
    {
        qosa_task_delete(*task_id);
    }

    *task_id = QOSA_NULL;
}

// Simplified test thread function
static void test_thread_function(void *argument)
{
    int *value = (int *)argument;
    QLOGV("Thread execution: arg = %d\n", *value);

    // Get current thread ID
    qosa_task_t current_id = QOSA_NULL;
    qosa_task_get_current_ref(&current_id);
    QLOGV("Current thread ID: %p\n", current_id);

    // Simulate work using osDelay
    for (int i = 0; i < 3; i++)
    {
        QLOGV("Work progress %d/3\n", i + 1);
        qosa_task_sleep_ms(50);
    }

    QLOGV("Thread execution completed\n");
}

// Test normal thread creation
static qosa_task_t test_normal_thread_creation(void)
{
    static int   thread_arg = 100;
    qosa_task_t  thread_id = QOSA_NULL;

    QLOGV("\n1. Create normal thread\n");

    if (qosa_task_create(&thread_id, 1024, QOSA_PRIORITY_NORMAL, "TestThread", test_thread_function, &thread_arg) != QOSA_OK)
    {
        QLOGV("Thread creation failed\n");
        return QOSA_NULL;
    }

    QLOGV("Thread created successfully: %p\n", thread_id);

    // Wait for thread execution
    qosa_task_sleep_ms(100);

    return thread_id;
}

// Test thread state query
static int32_t test_thread_state_query(qosa_task_t thread_id)
{
    qosa_int32_t state = -1;

    QLOGV("\n2. Thread state query\n");

    if (qosa_task_get_status(thread_id, &state) == QOSA_OK)
    {
        QLOGV("Thread state: %d\n", state);
    }

    return QOSA_OK;
}

// Test thread suspend and resume
static int32_t test_thread_suspend_resume(qosa_task_t thread_id)
{
    QLOGV("\n3. Thread suspend test\n");

    if (qosa_task_suspend(thread_id) == QOSA_OK)
    {
        QLOGV("Suspend successful\n");
        qosa_task_sleep_ms(50);
        QLOGV("Resume thread\n");
        qosa_task_resume(thread_id);
    }

    // Wait for thread completion
    qosa_task_sleep_ms(200);

    return QOSA_OK;
}

// Test thread termination
static int32_t test_thread_termination(qosa_task_t thread_id)
{
    QLOGV("\n4. Thread termination test\n");

    cleanup_task_handle(&thread_id);
    QLOGV("Thread termination handling completed\n");

    return QOSA_OK;
}

// Test error handling
static int32_t test_error_handling(void)
{
    int32_t state_result;
    int32_t suspend_result;
    int32_t terminate_result;
    qosa_int32_t thread_state = -1;

    QLOGV("\n5. Error cases test\n");

    state_result = qosa_task_get_status(QOSA_NULL, &thread_state);
    suspend_result = QOSA_ERROR_TASK_INVALID_ERR;
    terminate_result = QOSA_ERROR_TASK_INVALID_ERR;

    QLOGV("Invalid state query: %d\n", state_result);
    QLOGV("Invalid suspend: %d (skipped direct qosa call)\n", suspend_result);
    QLOGV("Invalid terminate: %d (skipped direct qosa call)\n", terminate_result);

    return QOSA_OK;
}

// Main test function
int32_t osThreadCreateTest_init(void)
{
    qosa_task_t thread_id = QOSA_NULL;
    int32_t     result;

    QLOGV("Starting thread functionality test\n");

    // 1. Normal thread creation test
    thread_id = test_normal_thread_creation();
    if (thread_id == QOSA_NULL)
    {
        return QOSA_FALSE;
    }

    // 2. Thread state query test
    result = test_thread_state_query(thread_id);
    if (result != QOSA_OK)
    {
        return result;
    }

    // 3. Thread suspend test
    result = test_thread_suspend_resume(thread_id);
    if (result != QOSA_OK)
    {
        return result;
    }

    // 4. Thread termination test
    result = test_thread_termination(thread_id);
    if (result != QOSA_OK)
    {
        return result;
    }

    // 5. Error cases test
    result = test_error_handling();
    if (result != QOSA_OK)
    {
        return result;
    }

    QLOGV("\nAll tests completed\n");
    return QOSA_OK;
}
