#include "qosa_sys.h"
#include "qosa_log.h"

#define BUFFER_SIZE 100

// 共享缓冲区和信号量
static char buffer[BUFFER_SIZE];
static qosa_sem_t empty_sem = NULL;    // 空槽位信号量
static qosa_sem_t full_sem = NULL;     // 数据信号量
static int producer_index = 0;
static int consumer_index = 0;
static int item_count = 0;

// 生产者任务
void producer_task(void *arg)
{
    char data = 'A';
    int ret;
    
    while (1) {
        // 等待空槽位
        ret = qosa_sem_wait(empty_sem, QOSA_WAIT_FOREVER);
        if (ret != QOSA_ERROR_OK) {
            QOSA_LOG_E("Producer", "Wait empty sem failed: %d", ret);
            continue;
        }
        
        // 生产数据
        buffer[producer_index] = data;
        producer_index = (producer_index + 1) % BUFFER_SIZE;
        item_count++;
        
        // 计算信号量计数值（用于监控）
        qosa_uint32_t empty_cnt, full_cnt;
        qosa_sem_get_cnt(empty_sem, &empty_cnt);
        qosa_sem_get_cnt(full_sem, &full_cnt);
        
        QOSA_LOG_I("Producer", "Produced '%c', items: %d, empty: %u, full: %u", 
                   data, item_count, empty_cnt, full_cnt);
        
        // 通知消费者
        ret = qosa_sem_release(full_sem);
        if (ret != QOSA_ERROR_OK) {
            QOSA_LOG_E("Producer", "Release full sem failed: %d", ret);
        }
        
        data = (data == 'Z') ? 'A' : data + 1;
        qosa_task_sleep(100); // 模拟生产时间
    }
}

// 消费者任务
void consumer_task(void *arg)
{
    char data;
    int ret;
    
    while (1) {
        // 等待数据
        ret = qosa_sem_wait(full_sem, 500); // 500ms超时
        if (ret == QOSA_ERROR_SEMA_TIMEOUT_ERR) {
            QOSA_LOG_W("Consumer", "Wait data timeout");
            continue;
        } else if (ret != QOSA_ERROR_OK) {
            QOSA_LOG_E("Consumer", "Wait full sem failed: %d", ret);
            continue;
        }
        
        // 消费数据
        data = buffer[consumer_index];
        consumer_index = (consumer_index + 1) % BUFFER_SIZE;
        item_count--;
        
        // 通知生产者有空槽位
        ret = qosa_sem_release(empty_sem);
        if (ret != QOSA_ERROR_OK) {
            QOSA_LOG_E("Consumer", "Release empty sem failed: %d", ret);
        }
        
        QOSA_LOG_I("Consumer", "Consumed '%c', remaining items: %d", data, item_count);
        
        qosa_task_sleep(150); // 模拟消费时间
    }
}

// 初始化生产者-消费者模型
int producer_consumer_init(void)
{
    int ret;
    
    // 1. 创建信号量
    // 空槽位信号量：初始为BUFFER_SIZE，最大为BUFFER_SIZE
    ret = qosa_sem_create_ex(&empty_sem, BUFFER_SIZE, BUFFER_SIZE);
    if (ret != QOSA_ERROR_OK) {
        QOSA_LOG_E("PC", "Create empty sem failed: %d", ret);
        return ret;
    }
    
    // 数据信号量：初始为0，最大为BUFFER_SIZE
    ret = qosa_sem_create_ex(&full_sem, 0, BUFFER_SIZE);
    if (ret != QOSA_ERROR_OK) {
        QOSA_LOG_E("PC", "Create full sem failed: %d", ret);
        qosa_sem_delete(empty_sem);
        return ret;
    }
    
    // 2. 创建生产者任务
    ret = qosa_task_create("Producer", producer_task, NULL, 
                          4096, QOSA_TASK_PRIORITY_NORMAL);
    if (ret != QOSA_ERROR_OK) {
        QOSA_LOG_E("PC", "Create producer task failed");
        goto cleanup;
    }
    
    // 3. 创建消费者任务
    ret = qosa_task_create("Consumer", consumer_task, NULL,
                          4096, QOSA_TASK_PRIORITY_NORMAL);
    if (ret != QOSA_ERROR_OK) {
        QOSA_LOG_E("PC", "Create consumer task failed");
        goto cleanup;
    }
    
    QOSA_LOG_I("PC", "Producer-Consumer model started");
    return QOSA_ERROR_OK;
    
cleanup:
    qosa_sem_delete(empty_sem);
    qosa_sem_delete(full_sem);
    return ret;
}