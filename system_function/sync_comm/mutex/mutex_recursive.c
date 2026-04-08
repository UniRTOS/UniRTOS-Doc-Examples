#include "qosa_sys.h"
#include "qosa_log.h"

static qosa_mutex_t recursive_mutex = NULL;

// 内部函数（需要锁保护）
static void internal_function(void)
{
    int ret;
    
    // 嵌套获取锁（递归锁允许）
    ret = qosa_mutex_lock(recursive_mutex, QOSA_WAIT_FOREVER);
    if (ret == QOSA_ERROR_OK) {
        QOSA_LOG_D("Recursive", "Lock acquired in internal_function");
        // 执行操作...
        qosa_mutex_unlock(recursive_mutex);
    }
}

// 外部函数（也需要锁保护）
void external_function(void)
{
    int ret;
    
    // 第一次获取锁
    ret = qosa_mutex_lock(recursive_mutex, QOSA_WAIT_FOREVER);
    if (ret == QOSA_ERROR_OK) {
        QOSA_LOG_D("Recursive", "Lock acquired in external_function");
        
        // 调用内部函数（嵌套获取锁）
        internal_function();
        
        // 释放锁（需要与获取次数匹配）
        qosa_mutex_unlock(recursive_mutex);
    }
}

// 递归锁示例初始化
int recursive_mutex_example(void)
{
    int ret;
    
    ret = qosa_mutex_create(&recursive_mutex);
    if (ret == QOSA_ERROR_OK) {
        // 创建任务执行递归锁测试
        qosa_task_create("RecursiveTest", external_function, NULL,
                        4096, QOSA_TASK_PRIORITY_NORMAL);
    }
    
    return ret;
}