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

// Test message structure
typedef struct
{
    int      id;
    char     data[32];
    uint32_t timestamp;
} test_msg_t;

static qosa_sem_t g_consumer_ready_sem = QOSA_NULL;

// Consumer thread function modification
static void msgq_consumer_thread(void *argument)
{
    qosa_msgq_t mq_id = (qosa_msgq_t)argument;
    test_msg_t  msg;
    int         result = QOSA_OK;
    QLOGV("Consumer thread started\n");

    // Release semaphore to indicate consumer has started
    qosa_sem_release(g_consumer_ready_sem);

    for (int i = 0; i < 3; i++)
    {
        // Get message from queue
        result = qosa_msgq_wait(mq_id, (qosa_uint8_t *)&msg, sizeof(msg), QOSA_WAIT_FOREVER);
        if (result == QOSA_OK)
        {
            QLOGV("Consumer received message: ID=%d, Data=%s, Time=%u\n", msg.id, msg.data, msg.timestamp);
        }
        else
        {
            QLOGE("Consumer failed to get message: %d\n", result);
        }
    }

    QLOGV("Consumer thread ended\n");
}

// Basic functionality test
static int32_t test_basic_operations(void)
{
    qosa_msgq_t  mq_id = QOSA_NULL;
    qosa_uint32_t msg_count = 0;
    // Basic send and receive test
    test_msg_t test_msg1 = {1, "Hello", 1000};
    test_msg_t test_msg2 = {2, "World", 2000};
    // Receive message
    test_msg_t received_msg;

    QLOGV("\n=== Basic Functionality Test ===\n");

    if (qosa_msgq_create(&mq_id, sizeof(test_msg_t), 5) != QOSA_OK)
    {
        QLOGV("Failed to create message queue\n");
        return QOSA_ERROR_MSGQ_CREATE_ERR;
    }
    QLOGV("Message queue created successfully: %p\n", mq_id);
    QLOGV("Message queue name: %s\n", "TestMsgQueue");

    // Send message
    QLOGV("Sending message1\n");
    if (qosa_msgq_release(mq_id, sizeof(test_msg_t), (qosa_uint8_t *)&test_msg1, QOSA_NO_WAIT) != QOSA_OK)
    {
        QLOGE("Failed to send message1\n");
        qosa_msgq_delete(mq_id);
        return -1;
    }
    if (qosa_msgq_get_cnt(mq_id, &msg_count) == QOSA_OK)
    {
        QLOGV("Current message count: %d\n", msg_count);
    }

    QLOGV("Sending message2\n");
    if (qosa_msgq_release(mq_id, sizeof(test_msg_t), (qosa_uint8_t *)&test_msg2, QOSA_NO_WAIT) != QOSA_OK)
    {
        QLOGE("Failed to send message2\n");
        qosa_msgq_delete(mq_id);
        return -1;
    }
    if (qosa_msgq_get_cnt(mq_id, &msg_count) == QOSA_OK)
    {
        QLOGV("Current message count: %d\n", msg_count);
    }

    if (qosa_msgq_wait(mq_id, (qosa_uint8_t *)&received_msg, sizeof(received_msg), QOSA_NO_WAIT) != QOSA_OK)
    {
        QLOGE("Failed to receive message\n");
        qosa_msgq_delete(mq_id);
        return -1;
    }
    QLOGV("Received message: ID=%d, Data=%s\n", received_msg.id, received_msg.data);
    if (qosa_msgq_get_cnt(mq_id, &msg_count) == QOSA_OK)
    {
        QLOGV("Message count after receive: %d\n", msg_count);
    }
    qosa_msgq_delete(mq_id);
    return 0;
}

// Multi-thread test function modification
static int32_t test_multithread(void)
{
    qosa_msgq_t  mq_id = QOSA_NULL;
    qosa_task_t  consumer_thread = QOSA_NULL;
    qosa_uint32_t msg_count = 0;
    QLOGV("\n=== Multi-thread Test ===\n");

    // Create semaphore
    if (qosa_sem_create_ex(&g_consumer_ready_sem, 0, 1) != QOSA_OK)
    {
        QLOGE("Failed to create semaphore\n");
        return -1;
    }

    // Increase message queue capacity
    if (qosa_msgq_create(&mq_id, sizeof(test_msg_t), 10) != QOSA_OK)
    {
        QLOGE("Failed to create message queue\n");
        qosa_sem_delete(g_consumer_ready_sem);
        return -1;
    }

    qosa_task_create(&consumer_thread, 2048, QOSA_PRIORITY_ABOVE_NORMAL, "MsgConsumer", msgq_consumer_thread, mq_id);

    // Wait for consumer thread to start (via semaphore)
    qosa_sem_wait(g_consumer_ready_sem, QOSA_WAIT_FOREVER);
    QLOGV("Consumer thread is ready\n");

    // Producer sends messages
    for (int i = 0; i < 3; i++)
    {
        test_msg_t new_msg = {.id = i + 10, .timestamp = 3000 + i * 100};
        int        status = QOSA_OK;
        snprintf(new_msg.data, sizeof(new_msg.data), "Message%d", i + 1);

        QLOGV("Producer sending message%d\n", i + 1);
        // Use finite timeout instead of infinite wait
        status = qosa_msgq_release(mq_id, sizeof(test_msg_t), (qosa_uint8_t *)&new_msg, 100);
        if (status != QOSA_OK)
        {
            QLOGE("Failed to send message: %d\n", status);
            // Wait for consumer to process existing messages
            qosa_task_sleep_ms(100);
            // Retry sending
            status = qosa_msgq_release(mq_id, sizeof(test_msg_t), (qosa_uint8_t *)&new_msg, 100);
            if (status != QOSA_OK)
            {
                QLOGE("Retry send failed: %d\n", status);
                break;
            }
        }
        if (qosa_msgq_get_cnt(mq_id, &msg_count) == QOSA_OK)
        {
            QLOGV("Message count after send: %d\n", msg_count);
        }

        // Increase send interval to ensure consumer has time to process
        qosa_task_sleep_ms(100);
    }

    // Wait for consumer to process all messages
    while (qosa_msgq_get_cnt(mq_id, &msg_count) == QOSA_OK && msg_count > 0)
    {
        qosa_task_sleep_ms(10);
    }

    // Ensure consumer thread completes
    qosa_task_sleep_ms(200);

    // Gracefully terminate thread and delete queue
    cleanup_task_handle(&consumer_thread);
    qosa_msgq_delete(mq_id);
    qosa_sem_delete(g_consumer_ready_sem);

    return 0;
}

// Boundary condition test
static int32_t test_boundary_conditions(void)
{
    qosa_msgq_t   mq_id = QOSA_NULL;
    qosa_uint32_t msg_count = 0;
    test_msg_t    empty_msg;
    int           get_result = QOSA_OK;
    QLOGV("\n=== Boundary Condition Test ===\n");

    if (qosa_msgq_create(&mq_id, sizeof(test_msg_t), 5) != QOSA_OK)
    {
        QLOGE("Failed to create message queue\n");
        return -1;
    }

    // Test queue full scenario
    QLOGV("Testing queue full...\n");
    for (int i = 0; i < 10; i++)
    {
        test_msg_t full_msg = {i, "Full queue test", 4000};
        int put_result = qosa_msgq_release(mq_id, sizeof(test_msg_t), (qosa_uint8_t *)&full_msg, 100);
        if (put_result != QOSA_OK)
        {
            QLOGE("%d-th send failed: %d (queue may be full)\n", i + 1, put_result);
            break;
        }
    }
    if (qosa_msgq_get_cnt(mq_id, &msg_count) == QOSA_OK)
    {
        QLOGV("Message count when queue full: %d\n", msg_count);
    }

    // Clear queue
    while (qosa_msgq_get_cnt(mq_id, &msg_count) == QOSA_OK && msg_count > 0)
    {
        test_msg_t temp_msg;
        qosa_msgq_wait(mq_id, (qosa_uint8_t *)&temp_msg, sizeof(temp_msg), QOSA_NO_WAIT);
    }
    if (qosa_msgq_get_cnt(mq_id, &msg_count) == QOSA_OK)
    {
        QLOGV("Message count after clear: %d\n", msg_count);
    }

    // Test queue empty scenario
    QLOGV("Testing queue empty...\n");
    get_result = qosa_msgq_wait(mq_id, (qosa_uint8_t *)&empty_msg, sizeof(empty_msg), 100);
    QLOGV("Get result from empty queue: %d\n", get_result);

    qosa_msgq_delete(mq_id);
    return 0;
}

// Error handling test
static int32_t test_error_handling(void)
{
    qosa_msgq_t   invalid_mq = QOSA_NULL;
    qosa_uint32_t msg_count = 0;
    QLOGV("\n=== Error Handling Test ===\n");

    test_msg_t test_msg = {1, "Test", 1000};

    // Test invalid parameters
    QLOGV("Invalid parameter creation: %s\n", qosa_msgq_create(&invalid_mq, sizeof(test_msg_t), 0) == QOSA_OK ? "Fail" : "Pass");
    if (invalid_mq != QOSA_NULL)
    {
        qosa_msgq_delete(invalid_mq);
    }
    QLOGV("Invalid message queue send: %d\n", qosa_msgq_release(QOSA_NULL, sizeof(test_msg_t), (qosa_uint8_t *)&test_msg, 100));
    QLOGV("Invalid message queue count: %d\n", qosa_msgq_get_cnt(QOSA_NULL, &msg_count));
    QLOGV("Message queue name query is not supported by qosa_msgq, skipped\n");

    return 0;
}

// Main test function
int32_t osMessageQueueTest_init(void)
{
    QLOGV("Starting message queue functionality test\n");

    //// Execute all tests
    if (test_basic_operations() != 0)
    {
        QLOGE("Basic functionality test failed\n");
        return -1;
    }

    if (test_multithread() != 0)
    {
        QLOGE("Multi-thread test failed\n");
        return -1;
    }

    if (test_boundary_conditions() != 0)
    {
        QLOGE("Boundary condition test failed\n");
        return -1;
    }

    if (test_error_handling() != 0)
    {
        QLOGE("Error handling test failed\n");
        return -1;
    }

    QLOGV("\nMessage queue test completed\n");
    return 0;
}
