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

// Event flags test thread function
static void event_flags_test_thread(void *argument)
{
    qosa_uint32_t result = 0;
    qosa_flag_t   ef_id = (qosa_flag_t)argument;
    QLOGV("Event flags test thread started\n");

    // Test 1: Wait for specific flags
    QLOGV("Waiting for flags 0x03 (any bit)...\n");
    result = qosa_flag_wait(ef_id, 0x03, QOSA_FLAG_CLEAR_ENABLE, QOSA_FLAG_WAIT_ANY, QOSA_WAIT_FOREVER);
    QLOGV("Received event flags: 0x%x\n", result);

    // Test 2: Wait for all flags
    QLOGV("Waiting for flags 0x07 (all bits)...\n");
    result = qosa_flag_wait(ef_id, 0x07, QOSA_FLAG_CLEAR_ENABLE, QOSA_FLAG_WAIT_ALL, QOSA_WAIT_FOREVER);
    QLOGV("Received all event flags: 0x%x\n", result);

    // Test 3: Test waiting without clearing flags
    QLOGV("Waiting for flags 0x01 (no clear)...\n");
    result = qosa_flag_wait(ef_id, 0x01, QOSA_FLAG_CLEAR_DISABLE, QOSA_FLAG_WAIT_ANY, 200);
    QLOGV("No clear wait result: 0x%x\n", result);

    QLOGV("Event flags test thread ended\n");
}

// Create event flags
static int32_t create_event_flags(qosa_flag_t *ef_id)
{
    QLOGV("\n1. Create event flags\n");

    if (qosa_flag_create(ef_id) != QOSA_ERROR_OK)
    {
        QLOGE("Failed to create event flags\n");
        return QOSA_FALSE;
    }
    QLOGV("Event flags created successfully: %p\n", *ef_id);

    return QOSA_TRUE;
}

// Create test thread
static int32_t create_test_thread(qosa_flag_t ef_id, qosa_task_t *thread_id)
{
    QLOGV("\n2. Create test thread\n");

    if (qosa_task_create(thread_id, 1024, QOSA_PRIORITY_NORMAL, "EventFlagsTestThread", event_flags_test_thread, ef_id) != QOSA_OK)
    {
        QLOGE("Failed to create test thread\n");
        return QOSA_FALSE;
    }

    return QOSA_TRUE;
}

// Test setting flags
static int32_t test_set_flags(qosa_flag_t ef_id)
{
    qosa_errcode_os_e set_result = QOSA_ERROR_OK;

    QLOGV("\n3. Test setting flags\n");
    qosa_task_sleep_ms(100);

    QLOGV("Setting flags 0x01\n");
    set_result = qosa_flag_set(ef_id, 0x01);
    QLOGV("Flags value after set: 0x%x\n", set_result);

    qosa_task_sleep_ms(100);

    QLOGV("Setting flags 0x02\n");
    set_result = qosa_flag_set(ef_id, 0x02);
    QLOGV("Flags value after set: 0x%x\n", set_result);

    qosa_task_sleep_ms(100);

    return QOSA_TRUE;
}

// Test clearing flags
static int32_t test_clear_flags(qosa_flag_t ef_id)
{
    qosa_errcode_os_e clear_result = QOSA_ERROR_OK;

    QLOGV("\n4. Test clearing flags\n");

    QLOGV("Clearing flags 0x01\n");
    clear_result = qosa_flag_clear(ef_id, 0x01);
    QLOGV("Flags value before clear: 0x%x\n", clear_result);

    // Set remaining required flags
    QLOGV("Setting flags 0x04\n");
    qosa_flag_set(ef_id, 0x04);

    qosa_task_sleep_ms(100);

    return QOSA_TRUE;
}

// Test timeout wait
static int32_t test_timeout_wait(qosa_flag_t ef_id)
{
    qosa_uint32_t timeout_result = 0;

    QLOGV("\n5. Test timeout wait\n");
    QLOGV("Waiting for non-existent flags 0x08 (should timeout)...\n");
    timeout_result = qosa_flag_wait(ef_id, 0x08, QOSA_FLAG_CLEAR_ENABLE, QOSA_FLAG_WAIT_ANY, 100);
    if (timeout_result == QOSA_ERROR_FLAG_WAIT_ERR)
    {
        QLOGE("Wait timeout, error code: 0x%x\n", timeout_result);
    }

    return QOSA_TRUE;
}

// Clean up test resources
static int32_t cleanup_test(qosa_task_t *thread_id, qosa_flag_t ef_id)
{
    qosa_errcode_os_e delete_result = QOSA_ERROR_OK;

    QLOGV("\n6. Cleanup test\n");

    // Wait for thread to complete
    qosa_task_sleep_ms(200);

    // Terminate test thread
    cleanup_task_handle(thread_id);
    QLOGV("Test thread cleanup completed\n");

    // Delete event flags
    delete_result = qosa_flag_delete(ef_id);
    QLOGV("Event flags delete result: %d\n", delete_result);

    return QOSA_TRUE;
}

// Error cases test
static int32_t test_error_cases(void)
{
    QLOGV("\n7. Error cases test\n");
    QLOGV("Null pointer set: 0x%x\n", qosa_flag_set(QOSA_NULL, 0x01));
    QLOGV("Null pointer clear: 0x%x\n", qosa_flag_clear(QOSA_NULL, 0x01));
    QLOGV("Null pointer wait: 0x%x\n", qosa_flag_wait(QOSA_NULL, 0x01, QOSA_FLAG_CLEAR_ENABLE, QOSA_FLAG_WAIT_ANY, 100));
    QLOGV("Null pointer delete: %d\n", qosa_flag_delete(QOSA_NULL));

    return QOSA_TRUE;
}

// Main test function
int32_t osEventFlagsTest_init(void)
{
    qosa_flag_t ef_id = QOSA_NULL;
    qosa_task_t thread_id = QOSA_NULL;

    QLOGV("Starting event flags functionality test\n");

    // Create event flags
    if (create_event_flags(&ef_id) != QOSA_TRUE)
    {
        QLOGE("Failed to create event flags\n");
        return QOSA_FALSE;
    }

    // Create test thread
    if (create_test_thread(ef_id, &thread_id) != QOSA_TRUE)
    {
        QLOGE("Failed to create test thread\n");
        qosa_flag_delete(ef_id);
        return QOSA_FALSE;
    }

    // Test setting flags
    if (test_set_flags(ef_id) != QOSA_TRUE)
    {
        QLOGE("Setting flags test failed\n");
        cleanup_task_handle(&thread_id);
        qosa_flag_delete(ef_id);
        return QOSA_FALSE;
    }

    // Test clearing flags
    if (test_clear_flags(ef_id) != QOSA_TRUE)
    {
        QLOGE("Clearing flags test failed\n");
        cleanup_task_handle(&thread_id);
        qosa_flag_delete(ef_id);
        return QOSA_FALSE;
    }

    // Test timeout wait
    if (test_timeout_wait(ef_id) != QOSA_TRUE)
    {
        QLOGE("Timeout wait test failed\n");
        cleanup_task_handle(&thread_id);
        qosa_flag_delete(ef_id);
        return QOSA_FALSE;
    }

    // Clean up test resources
    if (cleanup_test(&thread_id, ef_id) != QOSA_TRUE)
    {
        QLOGE("Failed to clean up test resources\n");
        return QOSA_FALSE;
    }

    // Error cases test
    if (test_error_cases() != QOSA_TRUE)
    {
        QLOGE("Error cases test failed\n");
        return QOSA_FALSE;
    }

    QLOGE("\nEvent flags test completed\n");
    return QOSA_OK;
}
