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

// Shared resource
static int          shared_counter = 0;
static qosa_mutex_t global_mutex = QOSA_NULL;
// --- Define task, mutex handle and attributes ---

// Mutex handle
static qosa_mutex_t test_mutex_id = QOSA_NULL;

// Task handles
static qosa_task_t low_prio_task_id = QOSA_NULL;
static qosa_task_t high_prio_task_id = QOSA_NULL;

// Basic test thread function
static void mutex_test_thread(void *argument)
{
    int thread_id = *(int *)argument;
    int old_value = 0;
    // Acquire lock
    if (qosa_mutex_lock(global_mutex, QOSA_WAIT_FOREVER) == QOSA_OK)
    {
        // Operate on shared resource
        old_value = shared_counter;
        qosa_task_sleep_ms(10);
        shared_counter = old_value + 1;
        QLOGV("Thread %d updated counter: %d -> %d\n", thread_id, old_value, shared_counter);

        // Release lock
        qosa_mutex_unlock(global_mutex);
    }
}

/**
 * @brief Low priority task (Task A)
 * @param argument Parameter passed to the task (unused)
 */
static void low_priority_task(void *argument)
{
    QOSA_UNUSED(argument);
    int status;

    QLOGV("Task A (Low priority): Starting, preparing to acquire mutex.\r\n");

    // 1. Acquire mutex
    status = qosa_mutex_lock(test_mutex_id, QOSA_WAIT_FOREVER);

    if (status == QOSA_OK)
    {
        QLOGV("Task A (Low priority): Successfully acquired mutex.\r\n");

        // 2. Hold mutex and perform some work, simulated with delay
        QLOGV("Task A (Low priority): Holding lock and delaying for 500ms...\r\n");
        qosa_task_sleep_ms(500);

        // 3. Release mutex
        QLOGV("Task A (Low priority): Preparing to release mutex.\r\n");
        status = qosa_mutex_unlock(test_mutex_id);
        if (status == QOSA_OK)
        {
            QLOGV("Task A (Low priority): Mutex released.\r\n");
        }
    }
    else
    {
        QLOGE("Task A (Low priority): Failed to acquire mutex, error code: %d\r\n", status);
    }

    QLOGV("Task A (Low priority): Execution completed and exiting.\r\n");
}

/**
 * @brief High priority task (Task B)
 * @param argument Parameter passed to the task (unused)
 */
static void high_priority_task(void *argument)
{
    QOSA_UNUSED(argument);
    int status;

    QLOGV("Task B (High priority): Starting.\r\n");

    // To ensure Task A runs first and gets the lock, delay slightly here
    qosa_task_sleep_ms(100);

    QLOGD("Task B (High priority): Preparing to acquire mutex, timeout set to 1000ms.\r\n");

    // 1. Try to acquire mutex, at this point the lock is held by Task A,
    //    Task B will enter blocked state
    //    Wait time set to 1000ms, greater than Task A's holding time of 500ms
    status = qosa_mutex_lock(test_mutex_id, 1000);

    QLOGD("---------------");

    // 2. Check acquisition result
    if (status == QOSA_OK)
    {
        QLOGV("Task B (High priority): Successfully acquired mutex!\r\n");

        // Calculate actual blocking time (in ms)

        // 3. Release mutex
        qosa_mutex_unlock(test_mutex_id);
        QLOGV("Task B (High priority): Mutex released.\r\n");
    }
    else if (status == QOSA_ERROR_MUTEX_EBUSY_ERR)
    {
        // This is a timeout scenario, which should not occur in this test case
        QLOGE("Task B (High priority): Mutex acquisition timeout!\r\n");
    }
    else
    {
        QLOGE("Task B (High priority): Failed to acquire mutex, error code: %d\r\n", status);
    }

    QLOGV("Task B (High priority): Execution completed and exiting.\r\n");
}

/**
 * @brief Application entry function - timeout test
 */
void app_main_01(void)
{
    int            thread_arg = 100;
    int            thread_arg1 = 100;
    QLOGV("--- Mutex blocking wait test ---\r\n");

    // Create mutex
    if (qosa_mutex_create(&test_mutex_id) != QOSA_OK)
    {
        QLOGE("Error: Failed to create mutex!\r\n");
        return;
    }
    QLOGV("Mutex created successfully.\r\n");

    qosa_task_create(&low_prio_task_id, 1024, QOSA_PRIORITY_NORMAL, "lowThread", low_priority_task, &thread_arg);

    qosa_task_create(&high_prio_task_id, 1024, QOSA_PRIORITY_HIGH, "HighThread", high_priority_task, &thread_arg1);

    if (low_prio_task_id == QOSA_NULL || high_prio_task_id == QOSA_NULL)
    {
        QLOGE("Error: Failed to create tasks!\r\n");
        return;
    }
    QLOGV("Tasks created successfully.\r\n");

    // Start kernel, begin task scheduling
    QLOGV("Starting scheduler...\r\n\r\n");
}

// Basic functionality test
static int32_t mutex_basic_test(void)
{
    QLOGV("\n2. Single-thread basic functionality test\n");
    if (qosa_mutex_lock(global_mutex, 100) == QOSA_OK)
    {
        QLOGV("Lock acquired successfully\n");
        shared_counter++;
        qosa_mutex_unlock(global_mutex);
        QLOGV("Lock released successfully\n");
        return QOSA_OK;
    }
    return QOSA_ERROR_MUTEX_LOCK_ERR;
}

// Timeout functionality test
static int32_t mutex_timeout_test(void)
{
    QLOGV("\n3. Timeout functionality test\n");
    app_main_01();
    qosa_task_sleep_ms(1200);
    low_prio_task_id = QOSA_NULL;
    high_prio_task_id = QOSA_NULL;
    if (test_mutex_id != QOSA_NULL)
    {
        qosa_mutex_delete(test_mutex_id);
        test_mutex_id = QOSA_NULL;
    }
    return QOSA_OK;
}

// Multi-thread test
static int32_t mutex_multithread_test(void)
{
    int            thread_ids[] = {1, 2, 3};
    qosa_task_t    threads[3] = {QOSA_NULL, QOSA_NULL, QOSA_NULL};
    int            i;

    QLOGV("\n4. Multi-thread test\n");
    QLOGV("Initial counter: %d\n", shared_counter);

    // Create and run test threads
    for (i = 0; i < 3; i++)
    {
        if (qosa_task_create(&threads[i], 1024, QOSA_PRIORITY_NORMAL, "MutexTestThread", mutex_test_thread, &thread_ids[i]) != QOSA_OK)
        {
            QLOGE("Thread %d creation failed\n", i + 1);
            return QOSA_ERROR_TASK_CREATE_ERR;
        }
    }

    // Wait for all threads to complete
    while (shared_counter < 3)
    {
        qosa_task_sleep_ms(10);
    }

    QLOGV("Final counter: %d (Expected value: 3)\n", shared_counter);

    return QOSA_OK;
}

// Error scenario test
static int32_t mutex_error_test(void)
{
    QLOGV("\n5. Error scenario test\n");
    QLOGV("Null pointer test:\n");
    QLOGV("  Acquire: %d\n", qosa_mutex_lock(QOSA_NULL, 100));
    QLOGV("  Release: %d\n", qosa_mutex_unlock(QOSA_NULL));
    QLOGV("  Delete: %d\n", qosa_mutex_delete(QOSA_NULL));
    return QOSA_OK;
}

// Cleanup test
static int32_t mutex_cleanup_test(void)
{
    QLOGV("\n6. Cleanup\n");
    int result = qosa_mutex_delete(global_mutex);
    QLOGV("Deletion result: %s\n", result == QOSA_OK ? "Success" : "Failure");
    global_mutex = QOSA_NULL;
    return result == QOSA_OK ? 0 : -1;
}

// Complete mutex test function
int32_t osMutexTest_init(void)
{
    QLOGV("Starting mutex functionality test\n");
    shared_counter = 0;

    // Phase 1: Create mutex
    QLOGV("\n1. Create mutex\n");
    if (qosa_mutex_create(&global_mutex) != QOSA_OK)
    {
        QLOGE("Error: Mutex creation failed\n");
        return QOSA_ERROR_MUTEX_CREATE_ERR;
    }
    QLOGV("Mutex created successfully\n");

    // Execute various tests
    if (mutex_basic_test() != 0 || mutex_timeout_test() != 0 || mutex_multithread_test() != 0 || mutex_error_test() != 0 || mutex_cleanup_test() != 0)
    {
        return QOSA_ERROR_MUTEX_LOCK_ERR;
    }

    QLOGV("\nTest completed\n");
    return QOSA_OK;
}
