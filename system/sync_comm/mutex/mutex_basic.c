#include "qosa_sys.h"
#include "qosa_log.h"

// 共享资源
static int shared_counter = 0;
static qosa_mutex_t counter_mutex = NULL;

// 任务1：增加计数器
void task1_counter_increment(void *arg)
{
    int ret;
    
    while (1) {
        // 获取锁（等待10ms）
        ret = qosa_mutex_lock(counter_mutex, 10);
        if (ret == QOSA_ERROR_OK) {
            // 临界区开始
            shared_counter++;
            QOSA_LOG_I("Task1", "Counter: %d", shared_counter);
            
            // 释放锁
            qosa_mutex_unlock(counter_mutex);
            // 临界区结束
        }
        
        qosa_task_sleep(100); // 休眠100ms
    }
}

// 任务2：减少计数器
void task2_counter_decrement(void *arg)
{
    int ret;
    
    while (1) {
        // 尝试获取锁（非阻塞）
        ret = qosa_mutex_try_lock(counter_mutex);
        if (ret == QOSA_ERROR_OK) {
            // 临界区开始
            shared_counter--;
            QOSA_LOG_I("Task2", "Counter: %d", shared_counter);
            
            // 释放锁
            qosa_mutex_unlock(counter_mutex);
            // 临界区结束
        } else if (ret == QOSA_ERROR_MUTEX_EBUSY_ERR) {
            QOSA_LOG_D("Task2", "Mutex busy, skip this time");
        }
        
        qosa_task_sleep(150); // 休眠150ms
    }
}

// 初始化函数
int mutex_example_init(void)
{
    int ret;
    
    // 1. 创建互斥锁
    ret = qosa_mutex_create(&counter_mutex);
    if (ret != QOSA_ERROR_OK) {
        QOSA_LOG_E("MutexExample", "Create mutex failed: %d", ret);
        return ret;
    }
    
    // 2. 创建任务1
    ret = qosa_task_create("Task1", task1_counter_increment, NULL, 
                          4096, QOSA_TASK_PRIORITY_NORMAL);
    if (ret != QOSA_ERROR_OK) {
        QOSA_LOG_E("MutexExample", "Create task1 failed");
        qosa_mutex_delete(counter_mutex);
        return ret;
    }
    
    // 3. 创建任务2
    ret = qosa_task_create("Task2", task2_counter_decrement, NULL,
                          4096, QOSA_TASK_PRIORITY_NORMAL);
    if (ret != QOSA_ERROR_OK) {
        QOSA_LOG_E("MutexExample", "Create task2 failed");
        qosa_mutex_delete(counter_mutex);
        return ret;
    }
    
    QOSA_LOG_I("MutexExample", "Mutex example started");
    return QOSA_ERROR_OK;
}

// 清理函数
void mutex_example_cleanup(void)
{
    if (counter_mutex != NULL) {
        qosa_mutex_delete(counter_mutex);
        counter_mutex = NULL;
    }
    QOSA_LOG_I("MutexExample", "Mutex example cleaned up");
}