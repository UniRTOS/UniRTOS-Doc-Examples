#include "qosa_sys.h"
#include "qosa_def.h"
#include "qosa_log.h"

// 定义flag标志位
#define FLAG_TASK1_READY   (1 << 0)
#define FLAG_TASK2_READY   (1 << 1)
#define FLAG_ALL_TASKS_READY (FLAG_TASK1_READY | FLAG_TASK2_READY)

static qosa_flag_t g_sync_flag = QOSA_NULL;

// 任务1函数
void task1_function(void *arg)
{
    QOSA_LOG_I("TASK1", "Task1 starting...");
    
    // 模拟初始化工作
    qosa_delay_ms(100);
    
    // 设置任务1就绪标志
    qosa_flag_set(g_sync_flag, FLAG_TASK1_READY);
    QOSA_LOG_I("TASK1", "Task1 ready flag set");
    
    // 继续执行其他工作
    // ...
}

// 任务2函数
void task2_function(void *arg)
{
    QOSA_LOG_I("TASK2", "Task2 starting...");
    
    // 模拟初始化工作
    qosa_delay_ms(200);
    
    // 设置任务2就绪标志
    qosa_flag_set(g_sync_flag, FLAG_TASK2_READY);
    QOSA_LOG_I("TASK2", "Task2 ready flag set");
    
    // 继续执行其他工作
    // ...
}

// 主任务函数
void main_task_function(void *arg)
{
    qosa_errcode_os_e ret;
    qosa_uint32_t received_flags;
    
    QOSA_LOG_I("MAIN", "Main task starting...");
    
    // 1. 创建flag对象
    ret = qosa_flag_create(&g_sync_flag);
    if (ret != QOSA_ERROR_OK) {
        QOSA_LOG_E("MAIN", "Failed to create flag: %d", ret);
        return;
    }
    QOSA_LOG_I("MAIN", "Flag created successfully");
    
    // 2. 创建并启动任务1和任务2
    // ... 这里省略任务创建代码
    
    // 3. 等待所有任务就绪
    QOSA_LOG_I("MAIN", "Waiting for all tasks to be ready...");
    received_flags = qosa_flag_wait(g_sync_flag, 
                                   FLAG_ALL_TASKS_READY,
                                   QOSA_FLAG_CLEAR_ENABLE,
                                   QOSA_FLAG_WAIT_ALL,
                                   5000); // 5秒超时
    
    if (received_flags == QOSA_ERROR_FLAG_WAIT_ERR) {
        QOSA_LOG_E("MAIN", "Timeout waiting for tasks to be ready");
    } else {
        QOSA_LOG_I("MAIN", "All tasks ready! Received flags: 0x%08X", received_flags);
        
        // 4. 手动清除特定标志位（示例）
        ret = qosa_flag_clear(g_sync_flag, FLAG_TASK1_READY);
        if (ret == QOSA_ERROR_OK) {
            QOSA_LOG_I("MAIN", "Task1 flag cleared");
        }
        
        // 验证flag已被清除
        received_flags = qosa_flag_wait(g_sync_flag, 
                                       FLAG_TASK1_READY,
                                       QOSA_FLAG_CLEAR_DISABLE,
                                       QOSA_FLAG_WAIT_ANY,
                                       100); // 短时等待
        
        if (received_flags == QOSA_ERROR_FLAG_WAIT_ERR) {
            QOSA_LOG_I("MAIN", "Task1 flag cleared successfully (timeout as expected)");
        }
    }
    
    // 5. 删除flag对象
    ret = qosa_flag_delete(g_sync_flag);
    if (ret == QOSA_ERROR_OK) {
        QOSA_LOG_I("MAIN", "Flag deleted successfully");
    } else {
        QOSA_LOG_E("MAIN", "Failed to delete flag: %d", ret);
    }
    
    QOSA_LOG_I("MAIN", "Main task completed");
}