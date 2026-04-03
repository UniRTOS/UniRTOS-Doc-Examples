#include "qosa_sys.h"
#include "qosa_log.h"

// 消息定义
typedef struct {
    qosa_uint32_t msg_id;
    qosa_uint32_t timestamp;
    qosa_int32_t  data;
    char          description[32];
} app_message_t;

static qosa_msgq_t g_msg_queue = NULL;
static const qosa_uint32_t MSG_QUEUE_SIZE = 10;  // 队列容量
static const qosa_uint32_t MSG_SIZE = sizeof(app_message_t);

// 生产者任务（多个实例）
void producer_task(void *arg)
{
    int task_id = *(int*)arg;
    app_message_t msg;
    int ret;
    qosa_uint32_t sequence = 0;
    
    while (1) {
        // 构造消息
        msg.msg_id = sequence++;
        msg.timestamp = qosa_get_system_time();
        msg.data = task_id * 1000 + sequence;
        snprintf(msg.description, sizeof(msg.description), 
                "Msg from Task%d-#%u", task_id, sequence);
        
        // 发送消息到队列（非阻塞，队列满则丢弃）
        ret = qosa_msgq_release(g_msg_queue, MSG_SIZE, (qosa_uint8_t*)&msg, 0);
        
        if (ret == QOSA_ERROR_OK) {
            qosa_uint32_t queue_cnt;
            qosa_msgq_get_cnt(g_msg_queue, &queue_cnt);
            
            QOSA_LOG_I("Producer%d", "Sent msg#%u, queue: %u/%u", 
                      task_id, msg.msg_id, queue_cnt, MSG_QUEUE_SIZE);
        } else if (ret == QOSA_ERROR_MSGQ_FULL_ERR) {
            QOSA_LOG_W("Producer%d", "Queue full, discard msg#%u", task_id, msg.msg_id);
        } else {
            QOSA_LOG_E("Producer%d", "Send failed: %d", task_id, ret);
        }
        
        qosa_task_sleep(200 + task_id * 50);  // 不同生产者发送间隔不同
    }
}

// 消费者任务（处理所有消息）
void consumer_task(void *arg)
{
    app_message_t msg;
    int ret;
    qosa_uint32_t processed_count = 0;
    
    while (1) {
        // 等待消息（带1秒超时）
        ret = qosa_msgq_wait(g_msg_queue, (qosa_uint8_t*)&msg, MSG_SIZE, 1000);
        
        if (ret == QOSA_ERROR_OK) {
            // 处理消息
            processed_count++;
            
            qosa_uint32_t queue_cnt;
            qosa_msgq_get_cnt(g_msg_queue, &queue_cnt);
            
            QOSA_LOG_I("Consumer", "Processing[%u]: ID=%u, From=%s, Data=%d, Queue=%u",
                      processed_count, msg.msg_id, msg.description, 
                      msg.data, queue_cnt);
            
            // 模拟消息处理时间
            qosa_task_sleep(300);
            
        } else if (ret == QOSA_ERROR_SEMA_TIMEOUT_ERR) {
            QOSA_LOG_D("Consumer", "No message for 1 second, waiting...");
        } else {
            QOSA_LOG_E("Consumer", "Receive error: %d", ret);
            qosa_task_sleep(1000);
        }
    }
}

// 监控任务（定期显示队列状态）
void monitor_task(void *arg)
{
    qosa_uint32_t last_cnt = 0;
    
    while (1) {
        qosa_uint32_t current_cnt;
        int ret = qosa_msgq_get_cnt(g_msg_queue, &current_cnt);
        
        if (ret == QOSA_ERROR_OK) {
            if (current_cnt != last_cnt) {
                float usage = (float)current_cnt / MSG_QUEUE_SIZE * 100;
                QOSA_LOG_I("Monitor", "Queue status: %u/%u (%.1f%%)", 
                          current_cnt, MSG_QUEUE_SIZE, usage);
                last_cnt = current_cnt;
            }
        }
        
        qosa_task_sleep(2000);  // 每2秒检查一次
    }
}

// 初始化消息队列系统
int message_queue_system_init(void)
{
    int ret;
    
    // 1. 创建消息队列
    ret = qosa_msgq_create(&g_msg_queue, MSG_SIZE, MSG_QUEUE_SIZE);
    if (ret != QOSA_ERROR_OK) {
        QOSA_LOG_E("System", "Create message queue failed: %d", ret);
        return ret;
    }
    
    QOSA_LOG_I("System", "Message queue created: size=%u bytes, capacity=%u",
              MSG_SIZE, MSG_QUEUE_SIZE);
    
    // 2. 创建3个生产者任务
    int producer_ids[] = {1, 2, 3};
    for (int i = 0; i < 3; i++) {
        char task_name[20];
        snprintf(task_name, sizeof(task_name), "Producer%d", producer_ids[i]);
        
        ret = qosa_task_create(task_name, producer_task, &producer_ids[i],
                              4096, QOSA_TASK_PRIORITY_NORMAL);
        if (ret != QOSA_ERROR_OK) {
            QOSA_LOG_E("System", "Create %s failed", task_name);
        } else {
            QOSA_LOG_I("System", "Producer task %d started", producer_ids[i]);
        }
    }
    
    // 3. 创建消费者任务
    ret = qosa_task_create("Consumer", consumer_task, NULL,
                          4096, QOSA_TASK_PRIORITY_NORMAL);
    if (ret != QOSA_ERROR_OK) {
        QOSA_LOG_E("System", "Create consumer task failed");
        goto cleanup;
    }
    
    // 4. 创建监控任务
    ret = qosa_task_create("Monitor", monitor_task, NULL,
                          2048, QOSA_TASK_PRIORITY_LOW);
    if (ret != QOSA_ERROR_OK) {
        QOSA_LOG_W("System", "Create monitor task failed (optional)");
    }
    
    QOSA_LOG_I("System", "Message queue system started successfully");
    return QOSA_ERROR_OK;
    
cleanup:
    if (g_msg_queue != NULL) {
        qosa_msgq_delete(g_msg_queue);
        g_msg_queue = NULL;
    }
    return ret;
}