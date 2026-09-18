#include "qosa_sys.h"
#include "qosa_log.h"

// 命令类型定义
typedef enum {
    CMD_NONE = 0,
    CMD_SYSTEM_RESET,
    CMD_DATA_UPDATE,
    CMD_STATUS_REPORT,
    CMD_NETWORK_CONNECT,
    CMD_NETWORK_DISCONNECT,
    CMD_MAX
} command_type_t;

// 命令消息结构
typedef struct {
    command_type_t cmd_type;
    qosa_uint32_t  cmd_id;
    qosa_uint32_t  timestamp;
    qosa_int32_t   param1;
    qosa_int32_t   param2;
    void*          extra_data;  // 可选额外数据
} command_message_t;

// 子系统定义
typedef enum {
    SUBSYS_NETWORK = 0,
    SUBSYS_DATA,
    SUBSYS_SYSTEM,
    SUBSYS_MAX
} subsystem_t;

static qosa_msgq_t g_cmd_queues[SUBSYS_MAX] = {NULL};

// 命令分发器（主控任务）
void command_dispatcher(void *arg)
{
    command_message_t master_cmd;
    int ret;
    
    // 创建主命令队列
    ret = qosa_msgq_create(&g_cmd_queues[0], sizeof(command_message_t), 20);
    if (ret != QOSA_ERROR_OK) {
        QOSA_LOG_E("Dispatcher", "Create master queue failed");
        return;
    }
    
    while (1) {
        // 等待命令（无限等待）
        ret = qosa_msgq_wait(g_cmd_queues[0], (qosa_uint8_t*)&master_cmd, 
                            sizeof(command_message_t), QOSA_WAIT_FOREVER);
        
        if (ret != QOSA_ERROR_OK) {
            QOSA_LOG_E("Dispatcher", "Receive command error: %d", ret);
            continue;
        }
        
        QOSA_LOG_I("Dispatcher", "Received CMD%d (ID:%u)", 
                  master_cmd.cmd_type, master_cmd.cmd_id);
        
        // 根据命令类型分发到对应子系统
        subsystem_t target_subsys;
        switch (master_cmd.cmd_type) {
            case CMD_NETWORK_CONNECT:
            case CMD_NETWORK_DISCONNECT:
                target_subsys = SUBSYS_NETWORK;
                break;
            case CMD_DATA_UPDATE:
                target_subsys = SUBSYS_DATA;
                break;
            case CMD_SYSTEM_RESET:
            case CMD_STATUS_REPORT:
                target_subsys = SUBSYS_SYSTEM;
                break;
            default:
                QOSA_LOG_W("Dispatcher", "Unknown command type: %d", master_cmd.cmd_type);
                continue;
        }
        
        // 转发命令到子系统队列
        if (g_cmd_queues[target_subsys] != NULL) {
            ret = qosa_msgq_release(g_cmd_queues[target_subsys], 
                                   sizeof(command_message_t),
                                   (qosa_uint8_t*)&master_cmd, 0);
            
            if (ret != QOSA_ERROR_OK) {
                QOSA_LOG_E("Dispatcher", "Forward to subsys%d failed: %d", 
                          target_subsys, ret);
            }
        }
    }
}

// 网络子系统任务
void network_subsystem(void *arg)
{
    command_message_t cmd;
    int ret;
    
    // 创建网络子系统队列
    ret = qosa_msgq_create(&g_cmd_queues[SUBSYS_NETWORK], 
                          sizeof(command_message_t), 10);
    if (ret != QOSA_ERROR_OK) {
        QOSA_LOG_E("Network", "Create queue failed");
        return;
    }
    
    QOSA_LOG_I("Network", "Subsystem started");
    
    while (1) {
        // 等待网络相关命令（带超时）
        ret = qosa_msgq_wait(g_cmd_queues[SUBSYS_NETWORK], 
                            (qosa_uint8_t*)&cmd,
                            sizeof(command_message_t), 5000);
        
        if (ret == QOSA_ERROR_SEMA_TIMEOUT_ERR) {
            // 超时，执行定期维护
            QOSA_LOG_D("Network", "No command, performing maintenance");
            continue;
        } else if (ret != QOSA_ERROR_OK) {
            QOSA_LOG_E("Network", "Receive error: %d", ret);
            continue;
        }
        
        // 处理命令
        switch (cmd.cmd_type) {
            case CMD_NETWORK_CONNECT:
                QOSA_LOG_I("Network", "Executing CONNECT command (ID:%u)", cmd.cmd_id);
                // 模拟网络连接操作
                qosa_task_sleep(1000);
                QOSA_LOG_I("Network", "Network connected");
                break;
                
            case CMD_NETWORK_DISCONNECT:
                QOSA_LOG_I("Network", "Executing DISCONNECT command (ID:%u)", cmd.cmd_id);
                // 模拟网络断开操作
                qosa_task_sleep(500);
                QOSA_LOG_I("Network", "Network disconnected");
                break;
                
            default:
                QOSA_LOG_W("Network", "Unexpected command: %d", cmd.cmd_type);
        }
    }
}

// 命令发送接口（供外部模块调用）
int send_command(command_type_t cmd_type, qosa_int32_t param1, qosa_int32_t param2)
{
    static qosa_uint32_t cmd_counter = 0;
    command_message_t cmd;
    int ret;
    
    if (g_cmd_queues[0] == NULL) {
        return QOSA_ERROR_MSGQ_INVALID_ERR;
    }
    
    // 构造命令
    cmd.cmd_type = cmd_type;
    cmd.cmd_id = cmd_counter++;
    cmd.timestamp = qosa_get_system_time();
    cmd.param1 = param1;
    cmd.param2 = param2;
    cmd.extra_data = NULL;
    
    // 发送到主命令队列
    ret = qosa_msgq_release(g_cmd_queues[0], sizeof(command_message_t),
                           (qosa_uint8_t*)&cmd, 0);
    
    if (ret == QOSA_ERROR_OK) {
        QOSA_LOG_D("CmdSender", "Command %d sent (ID:%u)", cmd_type, cmd.cmd_id);
    }
    
    return ret;
}

// 初始化命令系统
int command_system_init(void)
{
    int ret;
    
    // 启动分发器任务
    ret = qosa_task_create("Dispatcher", command_dispatcher, NULL,
                          4096, QOSA_TASK_PRIORITY_HIGH);
    if (ret != QOSA_ERROR_OK) {
        return ret;
    }
    
    // 启动网络子系统任务
    ret = qosa_task_create("Network", network_subsystem, NULL,
                          4096, QOSA_TASK_PRIORITY_NORMAL);
    if (ret != QOSA_ERROR_OK) {
        return ret;
    }
    
    // 启动其他子系统任务（略）
    
    QOSA_LOG_I("CmdSystem", "Command system initialized");
    return QOSA_ERROR_OK;
}

// 测试命令发送
void test_command_sender(void *arg)
{
    QOSA_LOG_I("Test", "Starting command test...");
    
    qosa_task_sleep(2000);
    
    // 发送网络连接命令
    send_command(CMD_NETWORK_CONNECT, 0, 0);
    
    qosa_task_sleep(3000);
    
    // 发送数据更新命令
    send_command(CMD_DATA_UPDATE, 100, 200);
    
    qosa_task_sleep(2000);
    
    // 发送网络断开命令
    send_command(CMD_NETWORK_DISCONNECT, 0, 0);
    
    QOSA_LOG_I("Test", "Command test completed");
}