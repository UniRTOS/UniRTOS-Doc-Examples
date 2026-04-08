#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <arpa/inet.h>
#include "qcm_socket_adp.h"

#define SERVER_PORT 8080       // 服务器监听端口
#define MAX_CLIENTS 10         // 最大客户端连接数
#define BUFFER_SIZE 1024       // 缓冲区大小
#define SERVER_NAME "TCP Server v1.0"

// 客户端连接信息结构体
typedef struct {
    int fd;                     // Socket文件描述符
    qosa_ip_addr_t ip_addr;     // 客户端IP地址
    int port;                   // 客户端端口
    pthread_t thread_id;        // 线程ID
    time_t connect_time;        // 连接时间
    int active;                 // 活跃状态
    char client_name[32];       // 客户端名称
} client_info_t;

// 服务器全局状态
typedef struct {
    int server_fd;              // 服务器Socket文件描述符
    volatile int running;       // 服务器运行状态
    int client_count;           // 当前客户端数量
    client_info_t clients[MAX_CLIENTS]; // 客户端数组
    pthread_t accept_thread;    // 接受连接线程
    pthread_t monitor_thread;   // 监控线程
    pthread_mutex_t lock;       // 互斥锁
} server_state_t;

// 全局服务器状态
static server_state_t g_server = {0};

/**
 * @brief 初始化服务器状态
 */
int init_server_state(void)
{
    memset(&g_server, 0, sizeof(g_server));
    
    // 初始化互斥锁
    if (pthread_mutex_init(&g_server.lock, NULL) != 0) {
        printf("初始化互斥锁失败\n");
        return -1;
    }
    
    g_server.running = 1;
    g_server.client_count = 0;
    
    // 初始化客户端数组
    for (int i = 0; i < MAX_CLIENTS; i++) {
        g_server.clients[i].fd = -1;
        g_server.clients[i].active = 0;
        memset(g_server.clients[i].client_name, 0, sizeof(g_server.clients[i].client_name));
    }
    
    return 0;
}

/**
 * @brief 创建服务器Socket
 */
int create_server_socket(int port)
{
    int server_fd;
    
    printf("创建服务器Socket...\n");
    
    // 创建TCP Socket
    server_fd = qcm_socket_create(
        0,                      // SIM卡ID
        0,                      // PDP上下文ID
        QCM_AF_INET,            // IPv4地址族
        QCM_SOCK_STREAM,        // TCP流式Socket
        QCM_TCP_PROTOCOL,       // TCP协议
        port,                   // 监听端口
        QOSA_TRUE               // 阻塞模式（监听Socket建议使用阻塞模式）
    );
    
    if (server_fd < 0) {
        printf("创建服务器Socket失败，错误码: %d\n", server_fd);
        return -1;
    }
    
    printf("服务器Socket创建成功，句柄: %d\n", server_fd);
    
    // 设置Socket选项：地址重用
    int reuse_addr = 1;
    int ret = qcm_socket_set_opt(server_fd, QCM_SOCK_REUSEADDR_OPT, &reuse_addr);
    if (ret < 0) {
        printf("设置地址重用失败，错误码: %d\n", ret);
    }
    
    // 设置系统选项：TCP保持连接
    int keepalive = 1;
    ret = qcm_socket_set_system_opt(QCM_TCP_KEEPALIVE_OPT, &keepalive);
    if (ret < 0) {
        printf("设置TCP保持连接失败，错误码: %d\n", ret);
    }
    
    // 设置系统选项：TCP窗口大小
    int recv_window = 64;  // 64KB
    ret = qcm_socket_set_system_opt(QCM_TCP_RECV_WINDOWS_OPT, &recv_window);
    if (ret < 0) {
        printf("设置接收窗口大小失败，错误码: %d\n", ret);
    }
    
    return server_fd;
}

/**
 * @brief 添加客户端到服务器
 */
int add_client(int client_fd, qosa_ip_addr_t *ip_addr, int port)
{
    pthread_mutex_lock(&g_server.lock);
    
    int index = -1;
    
    // 查找空闲的客户端槽位
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_server.clients[i].active == 0) {
            index = i;
            break;
        }
    }
    
    if (index == -1) {
        printf("已达到最大客户端连接数，无法接受新连接\n");
        pthread_mutex_unlock(&g_server.lock);
        return -1;
    }
    
    // 填充客户端信息
    g_server.clients[index].fd = client_fd;
    memcpy(&g_server.clients[index].ip_addr, ip_addr, sizeof(qosa_ip_addr_t));
    g_server.clients[index].port = port;
    g_server.clients[index].active = 1;
    g_server.clients[index].connect_time = time(NULL);
    
    // 生成客户端名称
    struct in_addr in_addr;
    in_addr.s_addr = ip_addr->addr;
    snprintf(g_server.clients[index].client_name, 
             sizeof(g_server.clients[index].client_name),
             "Client-%d-%s", index, inet_ntoa(in_addr));
    
    g_server.client_count++;
    
    pthread_mutex_unlock(&g_server.lock);
    
    printf("新客户端已添加: %s (索引: %d)\n", 
           g_server.clients[index].client_name, index);
    
    return index;
}

/**
 * @brief 从服务器移除客户端
 */
void remove_client(int index)
{
    if (index < 0 || index >= MAX_CLIENTS) {
        return;
    }
    
    pthread_mutex_lock(&g_server.lock);
    
    if (g_server.clients[index].active) {
        printf("正在移除客户端: %s\n", g_server.clients[index].client_name);
        
        // 关闭Socket
        if (g_server.clients[index].fd >= 0) {
            qcm_socket_close(g_server.clients[index].fd);
        }
        
        // 重置客户端信息
        g_server.clients[index].fd = -1;
        g_server.clients[index].active = 0;
        g_server.clients[index].thread_id = 0;
        memset(g_server.clients[index].client_name, 0, 
               sizeof(g_server.clients[index].client_name));
        
        g_server.client_count--;
        
        printf("客户端已移除，当前客户端数: %d\n", g_server.client_count);
    }
    
    pthread_mutex_unlock(&g_server.lock);
}

/**
 * @brief 处理客户端请求
 */
void* handle_client_thread(void* arg)
{
    int client_index = *(int*)arg;
    free(arg);
    
    if (client_index < 0 || client_index >= MAX_CLIENTS) {
        printf("无效的客户端索引\n");
        return NULL;
    }
    
    int client_fd = g_server.clients[client_index].fd;
    char client_name[64];
    
    // 获取客户端名称
    pthread_mutex_lock(&g_server.lock);
    strncpy(client_name, g_server.clients[client_index].client_name, 
            sizeof(client_name));
    pthread_mutex_unlock(&g_server.lock);
    
    printf("开始处理客户端: %s\n", client_name);
    
    char buffer[BUFFER_SIZE];
    
    while (g_server.running) {
        // 接收客户端消息
        memset(buffer, 0, sizeof(buffer));
        int ret = qcm_socket_read(client_fd, buffer, sizeof(buffer) - 1);
        
        if (ret < 0) {
            printf("从客户端 %s 接收数据失败，错误码: %d\n", client_name, ret);
            break;
        } else if (ret == 0) {
            printf("客户端 %s 断开连接\n", client_name);
            break;
        } else {
            buffer[ret] = '\0';
            printf("收到客户端 %s 的消息(%d字节): %s\n", client_name, ret, buffer);
            
            // 处理特殊命令
            if (strcmp(buffer, "quit") == 0 || strcmp(buffer, "exit") == 0) {
                printf("客户端 %s 请求退出\n", client_name);
                
                // 发送确认消息
                const char* response = "Goodbye!";
                qcm_socket_send(client_fd, (char*)response, strlen(response));
                break;
            }
            
            if (strcmp(buffer, "shutdown") == 0) {
                printf("客户端 %s 请求关闭服务器\n", client_name);
                g_server.running = 0;
                
                // 发送确认消息
                const char* response = "Server is shutting down...";
                qcm_socket_send(client_fd, (char*)response, strlen(response));
                break;
            }
            
            if (strcmp(buffer, "status") == 0) {
                // 返回服务器状态
                char status_msg[256];
                pthread_mutex_lock(&g_server.lock);
                snprintf(status_msg, sizeof(status_msg),
                        "Server: %s\nClients: %d/%d\nYour Name: %s",
                        SERVER_NAME, g_server.client_count, 
                        MAX_CLIENTS, client_name);
                pthread_mutex_unlock(&g_server.lock);
                
                qcm_socket_send(client_fd, status_msg, strlen(status_msg));
                continue;
            }
            
            if (strcmp(buffer, "time") == 0) {
                // 返回当前时间
                time_t now = time(NULL);
                char time_str[64];
                struct tm* tm_info = localtime(&now);
                strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
                
                qcm_socket_send(client_fd, time_str, strlen(time_str));
                continue;
            }
            
            if (strcmp(buffer, "help") == 0) {
                // 返回帮助信息
                const char* help_msg = 
                    "Available commands:\n"
                    "  help     - Show this help message\n"
                    "  status   - Show server status\n"
                    "  time     - Show current time\n"
                    "  quit     - Disconnect from server\n"
                    "  shutdown - Shutdown the server\n"
                    "  echo <text> - Echo back the text\n";
                
                qcm_socket_send(client_fd, help_msg, strlen(help_msg));
                continue;
            }
            
            // 处理echo命令
            if (strncmp(buffer, "echo ", 5) == 0) {
                qcm_socket_send(client_fd, buffer + 5, strlen(buffer + 5));
                continue;
            }
            
            // 默认回复
            char response[BUFFER_SIZE];
            snprintf(response, sizeof(response), 
                    "Server received: %s", buffer);
            
            ret = qcm_socket_send(client_fd, response, strlen(response));
            if (ret < 0) {
                printf("向客户端 %s 发送响应失败，错误码: %d\n", client_name, ret);
                break;
            }
        }
    }
    
    // 从服务器移除客户端
    remove_client(client_index);
    
    printf("客户端处理线程结束: %s\n", client_name);
    return NULL;
}

/**
 * @brief 接受客户端连接线程
 */
void* accept_clients_thread(void* arg)
{
    (void)arg;
    
    printf("开始接受客户端连接...\n");
    
    while (g_server.running) {
        qosa_ip_addr_t client_ip;
        int client_port;
        qosa_ip_addr_t local_ip;
        int local_port;
        
        printf("等待客户端连接...\n");
        
        // 接受客户端连接
        int client_fd = qcm_socket_accept(g_server.server_fd, 
                                          &client_ip, &client_port,
                                          &local_ip, &local_port);
        
        if (!g_server.running) {
            break;
        }
        
        if (client_fd < 0) {
            printf("接受客户端连接失败，错误码: %d\n", client_fd);
            continue;
        }
        
        // 显示客户端连接信息
        struct in_addr in_addr;
        in_addr.s_addr = client_ip.addr;
        printf("\n新客户端连接:\n");
        printf("  客户端地址: %s:%d\n", inet_ntoa(in_addr), client_port);
        printf("  客户端Socket: %d\n", client_fd);
        
        // 设置客户端Socket选项
        int timeout = 5000;  // 5秒超时
        qcm_socket_set_opt(client_fd, QCM_SOCK_OPT_RCVTIMEO, &timeout);
        
        // 添加到客户端列表
        int client_index = add_client(client_fd, &client_ip, client_port);
        if (client_index < 0) {
            printf("无法添加客户端到列表，关闭连接\n");
            qcm_socket_close(client_fd);
            continue;
        }
        
        // 创建客户端处理线程
        int* thread_arg = malloc(sizeof(int));
        if (!thread_arg) {
            printf("内存分配失败\n");
            remove_client(client_index);
            continue;
        }
        
        *thread_arg = client_index;
        
        int ret = pthread_create(&g_server.clients[client_index].thread_id, 
                                NULL, handle_client_thread, thread_arg);
        if (ret != 0) {
            printf("创建客户端处理线程失败，错误码: %d\n", ret);
            free(thread_arg);
            remove_client(client_index);
            continue;
        }
        
        // 发送欢迎消息
        const char* welcome_msg = "Welcome to TCP Server!\nType 'help' for available commands.";
        qcm_socket_send(client_fd, (char*)welcome_msg, strlen(welcome_msg));
        
        printf("客户端已接受，当前连接数: %d/%d\n", 
               g_server.client_count, MAX_CLIENTS);
    }
    
    printf("停止接受客户端连接\n");
    return NULL;
}

/**
 * @brief 服务器监控线程
 */
void* monitor_server_thread(void* arg)
{
    (void)arg;
    
    printf("服务器监控线程启动\n");
    
    while (g_server.running) {
        // 显示服务器状态
        printf("\n=== 服务器状态 ===\n");
        printf("服务器运行: %s\n", g_server.running ? "是" : "否");
        printf("客户端数量: %d/%d\n", g_server.client_count, MAX_CLIENTS);
        printf("运行时间: %ld 秒\n", (long)(time(NULL) - g_server.clients[0].connect_time));
        
        // 显示连接的客户端
        pthread_mutex_lock(&g_server.lock);
        if (g_server.client_count > 0) {
            printf("连接中的客户端:\n");
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (g_server.clients[i].active) {
                    struct in_addr in_addr;
                    in_addr.s_addr = g_server.clients[i].ip_addr.addr;
                    printf("  [%d] %s - %s:%d (连接时间: %lds)\n", 
                           i, g_server.clients[i].client_name,
                           inet_ntoa(in_addr), g_server.clients[i].port,
                           time(NULL) - g_server.clients[i].connect_time);
                }
            }
        }
        pthread_mutex_unlock(&g_server.lock);
        printf("===================\n");
        
        // 等待5秒
        for (int i = 0; i < 50 && g_server.running; i++) {
            usleep(100000);  // 100ms
        }
    }
    
    printf("服务器监控线程结束\n");
    return NULL;
}

/**
 * @brief 启动服务器
 */
int start_server(int port)
{
    printf("启动%s...\n", SERVER_NAME);
    
    // 初始化服务器状态
    if (init_server_state() < 0) {
        printf("初始化服务器状态失败\n");
        return -1;
    }
    
    // 创建服务器Socket
    g_server.server_fd = create_server_socket(port);
    if (g_server.server_fd < 0) {
        printf("创建服务器Socket失败\n");
        return -1;
    }
    
    // 开始监听
    int ret = qcm_socket_listen(g_server.server_fd, 5);
    if (ret < 0) {
        printf("监听端口失败，错误码: %d\n", ret);
        qcm_socket_close(g_server.server_fd);
        return -1;
    }
    
    printf("服务器开始在端口 %d 上监听\n", port);
    
    // 创建监控线程
    ret = pthread_create(&g_server.monitor_thread, NULL, 
                        monitor_server_thread, NULL);
    if (ret != 0) {
        printf("创建监控线程失败，错误码: %d\n", ret);
        qcm_socket_close(g_server.server_fd);
        return -1;
    }
    
    // 开始接受客户端连接
    accept_clients_thread(NULL);
    
    return 0;
}

/**
 * @brief 停止服务器
 */
void stop_server(void)
{
    printf("正在停止服务器...\n");
    
    g_server.running = 0;
    
    // 等待接受连接线程结束
    if (g_server.accept_thread) {
        pthread_join(g_server.accept_thread, NULL);
    }
    
    // 等待监控线程结束
    if (g_server.monitor_thread) {
        pthread_join(g_server.monitor_thread, NULL);
    }
    
    // 关闭所有客户端连接
    printf("关闭所有客户端连接...\n");
    pthread_mutex_lock(&g_server.lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_server.clients[i].active) {
            printf("关闭客户端: %s\n", g_server.clients[i].client_name);
            
            // 等待客户端线程结束
            if (g_server.clients[i].thread_id) {
                pthread_join(g_server.clients[i].thread_id, NULL);
            }
            
            // 关闭Socket
            if (g_server.clients[i].fd >= 0) {
                qcm_socket_close(g_server.clients[i].fd);
            }
        }
    }
    pthread_mutex_unlock(&g_server.lock);
    
    // 关闭服务器Socket
    if (g_server.server_fd >= 0) {
        printf("关闭服务器Socket...\n");
        qcm_socket_close(g_server.server_fd);
        g_server.server_fd = -1;
    }
    
    // 销毁互斥锁
    pthread_mutex_destroy(&g_server.lock);
    
    printf("服务器已停止\n");
}

/**
 * @brief 信号处理函数
 */
void signal_handler(int sig)
{
    printf("\n收到信号 %d，正在优雅关闭服务器...\n", sig);
    g_server.running = 0;
}

/**
 * @brief SSL/TLS服务器支持
 */
#ifdef CONFIG_QCM_VTLS_FUNC
void* handle_ssl_client_thread(void* arg)
{
    int client_index = *(int*)arg;
    free(arg);
    
    int client_fd = g_server.clients[client_index].fd;
    
    printf("为客户端 %s 初始化SSL...\n", 
           g_server.clients[client_index].client_name);
    
    // 配置SSL
    qcm_ssl_config_t ssl_config;
    memset(&ssl_config, 0, sizeof(ssl_config));
    
    // 设置服务器SSL证书等参数
    // ssl_config.ca_cert = "ca.crt";
    // ssl_config.server_cert = "server.crt";
    // ssl_config.server_key = "server.key";
    // ssl_config.verify_mode = SSL_VERIFY_NONE;
    
    int ret = qcm_socket_ssl_config(client_fd, &ssl_config, NULL, QOSA_TRUE);
    if (ret != 0) {
        printf("SSL配置失败，错误码: %d\n", ret);
        return NULL;
    }
    
    // SSL握手
    ret = qcm_socket_ssl_connect(client_fd);
    if (ret != 0) {
        printf("SSL握手失败，错误码: %d\n", ret);
        return NULL;
    }
    
    printf("SSL连接已建立，可以进行安全通信\n");
    
    // 这里可以进行SSL加密通信...
    char buffer[BUFFER_SIZE];
    const char* ssl_welcome = "Secure connection established!";
    qcm_socket_send(client_fd, (char*)ssl_welcome, strlen(ssl_welcome));
    
    while (g_server.running) {
        memset(buffer, 0, sizeof(buffer));
        ret = qcm_socket_read(client_fd, buffer, sizeof(buffer) - 1);
        
        if (ret <= 0) {
            break;
        }
        
        buffer[ret] = '\0';
        printf("收到SSL加密消息: %s\n", buffer);
        
        char response[BUFFER_SIZE];
        snprintf(response, sizeof(response), "SSL Echo: %s", buffer);
        qcm_socket_send(client_fd, response, strlen(response));
    }
    
    // 清理SSL资源
    qcm_socket_ssl_clean(client_fd);
    printf("SSL资源已清理\n");
    
    return NULL;
}
#endif

/**
 * @brief 命令行参数解析
 */
void parse_command_line(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
            if (i + 1 < argc) {
                SERVER_PORT = atoi(argv[i + 1]);
                i++;
            }
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("用法: %s [选项]\n", argv[0]);
            printf("选项:\n");
            printf("  -p, --port <端口>   指定服务器监听端口（默认: 8080）\n");
            printf("  -h, --help          显示帮助信息\n");
            exit(0);
        }
    }
}

/**
 * @brief 主函数
 */
int main(int argc, char *argv[])
{
    // 解析命令行参数
    parse_command_line(argc, argv);
    
    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("===== %s =====\n", SERVER_NAME);
    printf("监听端口: %d\n", SERVER_PORT);
    printf("最大客户端数: %d\n", MAX_CLIENTS);
#ifdef CONFIG_QCM_VTLS_FUNC
    printf("SSL/TLS支持: 已启用\n");
#else
    printf("SSL/TLS支持: 未启用\n");
#endif
    printf("================\n\n");
    
    // 启动服务器
    if (start_server(SERVER_PORT) < 0) {
        printf("服务器启动失败\n");
        return -1;
    }
    
    // 主循环
    printf("服务器运行中，按 Ctrl+C 停止...\n");
    
    while (g_server.running) {
        sleep(1);
    }
    
    // 停止服务器
    stop_server();
    
    printf("服务器程序结束\n");
    return 0;
}

/**
 * @brief 简单的单线程服务器示例
 */
int simple_server(int port)
{
    int server_fd = create_server_socket(port);
    if (server_fd < 0) {
        return -1;
    }
    
    if (qcm_socket_listen(server_fd, 5) < 0) {
        qcm_socket_close(server_fd);
        return -1;
    }
    
    printf("简单服务器在端口 %d 上运行\n", port);
    
    while (1) {
        qosa_ip_addr_t client_ip;
        int client_port;
        qosa_ip_addr_t local_ip;
        int local_port;
        
        printf("等待客户端连接...\n");
        
        int client_fd = qcm_socket_accept(server_fd, &client_ip, &client_port,
                                         &local_ip, &local_port);
        if (client_fd < 0) {
            printf("接受连接失败\n");
            continue;
        }
        
        struct in_addr in_addr;
        in_addr.s_addr = client_ip.addr;
        printf("客户端连接: %s:%d\n", inet_ntoa(in_addr), client_port);
        
        // 处理客户端
        char buffer[256];
        const char* welcome = "Welcome to Simple Server!";
        qcm_socket_send(client_fd, (char*)welcome, strlen(welcome));
        
        while (1) {
            memset(buffer, 0, sizeof(buffer));
            int ret = qcm_socket_read(client_fd, buffer, sizeof(buffer) - 1);
            
            if (ret <= 0) {
                printf("客户端断开连接\n");
                break;
            }
            
            buffer[ret] = '\0';
            printf("收到: %s\n", buffer);
            
            char response[256];
            snprintf(response, sizeof(response), "Echo: %s", buffer);
            qcm_socket_send(client_fd, response, strlen(response));
            
            if (strcmp(buffer, "quit") == 0) {
                printf("客户端请求退出\n");
                break;
            }
        }
        
        qcm_socket_close(client_fd);
        printf("客户端连接已关闭\n");
    }
    
    qcm_socket_close(server_fd);
    return 0;
}