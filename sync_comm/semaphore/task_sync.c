#include "qosa_sys.h"
#include "qosa_log.h"

#define MAX_CONNECTIONS 5

// 连接池和信号量
static qosa_sem_t connection_sem = NULL;
static int active_connections = 0;

// 模拟数据库连接结构
typedef struct {
    int id;
    char status[20];
} db_connection_t;

static db_connection_t connections[MAX_CONNECTIONS];

// 获取数据库连接
int acquire_connection(int timeout_ms)
{
    int ret;
    
    // 等待可用连接
    ret = qosa_sem_wait(connection_sem, timeout_ms);
    if (ret == QOSA_ERROR_SEMA_TIMEOUT_ERR) {
        QOSA_LOG_W("DB", "No available connection, timeout");
        return -1;
    } else if (ret != QOSA_ERROR_OK) {
        QOSA_LOG_E("DB", "Wait connection sem failed: %d", ret);
        return -1;
    }
    
    // 查找空闲连接
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (strcmp(connections[i].status, "idle") == 0) {
            connections[i].id = i;
            strcpy(connections[i].status, "busy");
            active_connections++;
            
            qosa_uint32_t available;
            qosa_sem_get_cnt(connection_sem, &available);
            QOSA_LOG_I("DB", "Connection %d acquired, active: %d, available: %u", 
                      i, active_connections, available);
            
            return i;
        }
    }
    
    // 不应该执行到这里
    qosa_sem_release(connection_sem);
    return -1;
}

// 释放数据库连接
void release_connection(int conn_id)
{
    if (conn_id < 0 || conn_id >= MAX_CONNECTIONS) {
        QOSA_LOG_E("DB", "Invalid connection ID: %d", conn_id);
        return;
    }
    
    strcpy(connections[conn_id].status, "idle");
    active_connections--;
    
    // 释放信号量
    int ret = qosa_sem_release(connection_sem);
    if (ret != QOSA_ERROR_OK) {
        QOSA_LOG_E("DB", "Release connection sem failed: %d", ret);
    }
    
    qosa_uint32_t available;
    qosa_sem_get_cnt(connection_sem, &available);
    QOSA_LOG_I("DB", "Connection %d released, active: %d, available: %u", 
              conn_id, active_connections, available);
}

// 数据库操作任务
void db_task(void *arg)
{
    int task_id = *(int*)arg;
    int conn_id;
    
    for (int i = 0; i < 3; i++) {  // 每个任务执行3次操作
        // 获取连接（等待2秒）
        conn_id = acquire_connection(2000);
        if (conn_id < 0) {
            QOSA_LOG_W("Task%d", "Failed to get connection", task_id);
            continue;
        }
        
        // 模拟数据库操作
        QOSA_LOG_I("Task%d", "Using connection %d for DB operation", task_id, conn_id);
        qosa_task_sleep(300 + task_id * 50);  // 不同任务不同操作时间
        
        // 释放连接
        release_connection(conn_id);
        
        qosa_task_sleep(200);  // 任务间间隔
    }
}

// 初始化连接池
int connection_pool_init(void)
{
    int ret;
    
    // 1. 初始化连接池
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        connections[i].id = i;
        strcpy(connections[i].status, "idle");
    }
    
    // 2. 创建连接信号量（初始全部可用）
    ret = qosa_sem_create_ex(&connection_sem, MAX_CONNECTIONS, MAX_CONNECTIONS);
    if (ret != QOSA_ERROR_OK) {
        QOSA_LOG_E("Pool", "Create connection sem failed: %d", ret);
        return ret;
    }
    
    // 3. 创建多个数据库任务
    int task_ids[] = {1, 2, 3, 4, 5, 6, 7, 8};  // 8个任务竞争5个连接
    
    for (int i = 0; i < 8; i++) {
        char task_name[20];
        snprintf(task_name, sizeof(task_name), "DBTask%d", task_ids[i]);
        
        ret = qosa_task_create(task_name, db_task, &task_ids[i],
                              4096, QOSA_TASK_PRIORITY_NORMAL);
        if (ret != QOSA_ERROR_OK) {
            QOSA_LOG_E("Pool", "Create task %s failed", task_name);
        }
    }
    
    QOSA_LOG_I("Pool", "Connection pool started with %d connections", MAX_CONNECTIONS);
    return QOSA_ERROR_OK;
}