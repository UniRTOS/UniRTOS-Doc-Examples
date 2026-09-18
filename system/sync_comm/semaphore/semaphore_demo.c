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

// Global variables
static int        shared_resource = 0;
static qosa_sem_t global_sem = QOSA_NULL;

static int32_t test_basic_semaphore(void)
{
    QLOGD("\n=== Basic Semaphore Test ===\n");
    shared_resource = 0;

    if (qosa_sem_create_ex(&global_sem, 1, 1) != QOSA_OK)
    {
        QLOGE("Failed to create semaphore\n");
        return QOSA_ERROR_SEMA_CREATE_ERR;
    }

    QLOGI("Semaphore created successfully: %p\n", global_sem);
    {
        qosa_uint32_t sem_count = 0;
        qosa_sem_get_cnt(global_sem, &sem_count);
        QLOGI("Initial semaphore count: %d\n", sem_count);
    }
    return QOSA_OK;
}

static void semaphore_test_thread(void *argument)
{
    int thread_id = *(int *)argument;
    QLOGI("Thread %d started\n", thread_id);

    for (int i = 0; i < 2; i++)
    {
        QLOGD("Thread %d trying to acquire semaphore...\n", thread_id);
        int acquire_result = qosa_sem_wait(global_sem, QOSA_WAIT_FOREVER);

        if (acquire_result == QOSA_OK)
        {
            QLOGD("Thread %d acquired semaphore, operating on shared resource...\n", thread_id);
            int temp = shared_resource;
            qosa_task_sleep_ms(20);
            shared_resource = temp + 1;
            QLOGD("Thread %d completed operation, shared resource: %d\n", thread_id, shared_resource);

            qosa_sem_release(global_sem);
            QLOGD("Thread %d released semaphore\n", thread_id);
        }
        else
        {
            QLOGE("Thread %d failed to acquire semaphore: %d\n", thread_id, acquire_result);
        }

        qosa_task_sleep_ms(30);
    }

    QLOGI("Thread %d ended\n", thread_id);
}

//Multi-thread Competition Test
static int32_t test_multi_thread_competition(void)
{
    QLOGI("\n=== Multi-thread Competition Test ===\n");
    QLOGI("Initial shared resource: %d\n", shared_resource);

    int            thread_ids[] = {1, 2, 3};
    qosa_task_t    threads[3] = {QOSA_NULL, QOSA_NULL, QOSA_NULL};
    for (int i = 0; i < 3; i++)
    {
        QLOGI("Creating thread %d\n", i + 1);
        if (qosa_task_create(&threads[i], 1024, QOSA_PRIORITY_NORMAL, "SemTestThread", semaphore_test_thread, &thread_ids[i]) != QOSA_OK)
        {
            QLOGE("Failed to create thread %d\n", i + 1);
            // Clean up already created threads
            for (int j = 0; j < i; j++)
            {
                cleanup_task_handle(&threads[j]);
            }
            return QOSA_ERROR_TASK_CREATE_ERR;
        }
        qosa_task_sleep_ms(10);
    }

    // Wait for threads to complete
    QLOGI("Waiting for all threads to execute...\n");
    for (int i = 0; i < 10; i++)
    {
        qosa_uint32_t sem_count = 0;
        qosa_task_sleep_ms(100);
        qosa_sem_get_cnt(global_sem, &sem_count);
        QLOGD("Current shared resource: %d, semaphore count: %d\n", shared_resource, sem_count);

        if (shared_resource >= 6)
        {
            break;
        }
    }

    // Clean up threads
    for (int i = 0; i < 3; i++)
    {
        cleanup_task_handle(&threads[i]);
    }

    QLOGI("Final shared resource: %d (expected: 6)\n", shared_resource);
    return QOSA_OK;
}

// Timeout Acquire Test
static int32_t test_timeout_acquire(void)
{
    QLOGI("\n=== Timeout Acquire Test ===\n");
    int         result = QOSA_OK;
    qosa_sem_t  timeout_sem = QOSA_NULL;
    if (qosa_sem_create_ex(&timeout_sem, 0, 1) != QOSA_OK)
    {
        QLOGE("Failed to create timeout test semaphore\n");
        return QOSA_ERROR_SEMA_CREATE_ERR;
    }

    QLOGI("Attempting to acquire semaphore (should timeout)...\n");
    result = qosa_sem_wait(timeout_sem, 100);
    QLOGI("Timeout test result: %s\n", result == QOSA_ERROR_SEMA_TIMEOUT_ERR ? "PASSED" : "FAILED");

    qosa_sem_delete(timeout_sem);
    return QOSA_OK;
}

// counting semaphore test
static int32_t test_counting_semaphore(void)
{
    qosa_uint32_t sem_count = 0;
    QLOGI("\n=== Counting Semaphore Test ===\n");

    qosa_sem_t counting_sem = QOSA_NULL;
    if (qosa_sem_create_ex(&counting_sem, 2, 3) != QOSA_OK)
    {
        QLOGE("Failed to create counting semaphore\n");
        return QOSA_ERROR_SEMA_CREATE_ERR;
    }

    qosa_sem_get_cnt(counting_sem, &sem_count);
    QLOGI("Initial count: %d\n", sem_count);

    // Acquire twice
    qosa_sem_wait(counting_sem, QOSA_NO_WAIT);
    qosa_sem_get_cnt(counting_sem, &sem_count);
    QLOGI("Count after first acquire: %d\n", sem_count);
    qosa_sem_wait(counting_sem, QOSA_NO_WAIT);
    qosa_sem_get_cnt(counting_sem, &sem_count);
    QLOGI("Count after second acquire: %d\n", sem_count);

    // Third acquire should fail
    int third_acquire = qosa_sem_wait(counting_sem, 50);
    QLOGI("Third acquire result: %s\n", third_acquire == QOSA_ERROR_SEMA_TIMEOUT_ERR ? "PASSED" : "FAILED");

    // Release once
    qosa_sem_release(counting_sem);
    qosa_sem_get_cnt(counting_sem, &sem_count);
    QLOGI("Count after release: %d\n", sem_count);

    qosa_sem_delete(counting_sem);
    return QOSA_OK;
}

// error handling test
static int32_t test_error_handling(void)
{
    qosa_sem_t   invalid_sem = QOSA_NULL;
    qosa_sem_t   invalid_init_sem = QOSA_NULL;
    qosa_uint32_t sem_count = 0;
    QLOGI("\n=== Error Handling Test ===\n");

    QLOGI("Invalid parameter creation: %s\n", qosa_sem_create_ex(&invalid_sem, 1, 0) == QOSA_OK ? "FAILED" : "PASSED");
    if (invalid_sem != QOSA_NULL)
    {
        qosa_sem_delete(invalid_sem);
    }
    QLOGI("Initial count greater than maximum: %s\n", qosa_sem_create_ex(&invalid_init_sem, 2, 1) == QOSA_OK ? "FAILED" : "PASSED");
    if (invalid_init_sem != QOSA_NULL)
    {
        qosa_sem_delete(invalid_init_sem);
    }
    QLOGI("Invalid semaphore acquire: %d\n", qosa_sem_wait(QOSA_NULL, 100));
    QLOGI("Invalid semaphore release: %d\n", qosa_sem_release(QOSA_NULL));
    QLOGI("Invalid semaphore count: %d\n", qosa_sem_get_cnt(QOSA_NULL, &sem_count));
    QLOGI("Invalid semaphore delete: %d\n", qosa_sem_delete(QOSA_NULL));

    return QOSA_OK;
}

// main function test
int32_t osSemaphoreTest_init(void)
{
    QLOGI("Starting semaphore functionality test\n");

    // Execute all tests
    if (test_basic_semaphore() != 0)
    {
        QLOGE("Basic semaphore test failed\n");
        return QOSA_ERROR_SEMA_CREATE_ERR;
    }

    if (test_multi_thread_competition() != 0)
    {
        QLOGE("Multi-thread competition test failed\n");
        return QOSA_ERROR_TASK_CREATE_ERR;
    }

    if (test_timeout_acquire() != 0)
    {
        QLOGE("Timeout acquire test failed\n");
        return QOSA_ERROR_SEMA_TIMEOUT_ERR;
    }

    if (test_counting_semaphore() != 0)
    {
        QLOGE("Counting semaphore test failed\n");
        return QOSA_ERROR_SEMA_CREATE_ERR;
    }

    if (test_error_handling() != 0)
    {
        QLOGE("Error handling test failed\n");
        return QOSA_ERROR_SEMA_INVALID_ERR;
    }

    // Clean up global semaphore
    QLOGI("\n=== Cleanup Test ===\n");
    int delete_result = qosa_sem_delete(global_sem);
    QLOGI("Semaphore deletion result: %d\n", delete_result);

    QLOGI("\nSemaphore test completed\n");
    return QOSA_OK;
}
