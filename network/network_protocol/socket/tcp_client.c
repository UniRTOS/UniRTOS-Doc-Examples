#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "qcm_socket_adp.h"

#define SERVER_IP "127.0.0.1"  // 服务器IP地址
#define SERVER_PORT 8080       // 服务器端口号
#define BUFFER_SIZE 1024

// 全局变量
static volatile int running = 1;
static int client_socket = -1;

/**
 * @brief 信号处理函数，用于优雅退出
 */
void signal_handler(int sig)
{
    printf("\n收到信号 %d，正在退出...\n", sig);
    running = 0;
}

/**
 * @brief 显示连接状态信息
 */
void display_connection_status(int sock_fd)
{
    printf("Socket状态信息:\n");
    printf("  句柄: %d\n", sock_fd);
    printf("  状态: 已连接\n");
}

/**
 * @brief 连接服务器函数
 */
int connect_to_server(const char *server_ip, int server_port)
{
    int ret = 0;
    qosa_ip_addr_t server_addr;
    struct in_addr in_addr;
    
    printf("尝试连接服务器 %s:%d...\n", server_ip, server_port);
    
    // 1. 转换IP地址字符串为网络字节序
    if (inet_pton(AF_INET, server_ip, &in_addr) <= 0) {
        printf("IP地址转换失败: %s\n", server_ip);
        return -1;
    }
    
    // 2. 将转换后的地址赋值给server_addr
    // 注意：这里假设qosa_ip_addr_t结构体包含addr成员
    server_addr.addr = in_addr.s_addr;
    
    // 3. 创建TCP Socket
    client_socket = qcm_socket_create(
        0,                      // SIM卡ID
        0,                      // PDP上下文ID
        QCM_AF_INET,            // IPv4地址族
        QCM_SOCK_STREAM,        // TCP流式Socket
        QCM_TCP_PROTOCOL,       // TCP协议
        0,                      // 本地端口（0表示系统自动分配）
        QOSA_TRUE               // 阻塞模式
    );
    
    if (client_socket < 0) {
        printf("创建Socket失败，错误码: %d\n", client_socket);
        return -1;
    }
    printf("Socket创建成功，句柄: %d\n", client_socket);
    
    // 4. 设置Socket选项
    // 设置接收超时（5秒）
    int timeout = 5000;  // 5秒，单位毫秒
    ret = qcm_socket_set_opt(client_socket, QCM_SOCK_OPT_RCVTIMEO, &timeout);
    if (ret < 0) {
        printf("设置接收超时失败，错误码: %d\n", ret);
    }
    
    // 5. 连接服务器
    ret = qcm_socket_connect(client_socket, &server_addr, server_port);
    if (ret < 0) {
        printf("连接服务器失败，错误码: %d\n", ret);
        qcm_socket_close(client_socket);
        client_socket = -1;
        return -1;
    }
    
    printf("成功连接到服务器 %s:%d\n", server_ip, server_port);
    display_connection_status(client_socket);
    
    return 0;
}

/**
 * @brief 发送消息到服务器
 */
int send_message_to_server(const char *message)
{
    if (!message || client_socket < 0) {
        printf("无效参数或未连接到服务器\n");
        return -1;
    }
    
    int len = strlen(message);
    int ret = qcm_socket_send(client_socket, (char *)message, len);
    
    if (ret < 0) {
        printf("发送失败，错误码: %d\n", ret);
        return -1;
    } else if (ret != len) {
        printf("部分发送成功: %d/%d 字节\n", ret, len);
    } else {
        printf("消息发送成功: %d 字节\n", ret);
    }
    
    return ret;
}

/**
 * @brief 接收服务器响应
 */
int receive_server_response(char *buffer, int buffer_size)
{
    if (!buffer || buffer_size <= 0 || client_socket < 0) {
        printf("无效参数或未连接到服务器\n");
        return -1;
    }
    
    memset(buffer, 0, buffer_size);
    int ret = qcm_socket_read(client_socket, buffer, buffer_size - 1);
    
    if (ret < 0) {
        printf("接收失败，错误码: %d\n", ret);
        return -1;
    } else if (ret == 0) {
        printf("服务器关闭了连接\n");
        return 0;
    } else {
        buffer[ret] = '\0';  // 确保字符串以NULL结尾
        printf("收到服务器响应: %s (%d字节)\n", buffer, ret);
        return ret;
    }
}

/**
 * @brief 交互式聊天模式
 */
void interactive_chat_mode(void)
{
    char input[256];
    char response[BUFFER_SIZE];
    
    printf("\n=== 交互式聊天模式 ===\n");
    printf("输入消息发送到服务器，输入'quit'退出\n");
    printf("================================\n");
    
    while (running) {
        printf("\n请输入消息: ");
        fgets(input, sizeof(input), stdin);
        
        // 去除换行符
        input[strcspn(input, "\n")] = 0;
        
        if (strlen(input) == 0) {
            continue;
        }
        
        // 检查退出命令
        if (strcmp(input, "quit") == 0 || strcmp(input, "exit") == 0) {
            printf("退出聊天模式\n");
            break;
        }
        
        // 发送消息
        int ret = send_message_to_server(input);
        if (ret < 0) {
            printf("发送失败，连接可能已断开\n");
            break;
        }
        
        // 接收响应
        ret = receive_server_response(response, sizeof(response));
        if (ret <= 0) {
            printf("接收失败或连接关闭\n");
            break;
        }
        
        // 等待一段时间（避免过快发送）
        usleep(100000);  // 100ms
    }
}

/**
 * @brief 自动测试模式
 */
void auto_test_mode(void)
{
    const char *test_messages[] = {
        "Hello, Server!",
        "How are you today?",
        "This is a test message",
        "Test message 4",
        "Final test message",
        NULL
    };
    
    char response[BUFFER_SIZE];
    int message_count = 0;
    
    printf("\n=== 自动测试模式 ===\n");
    printf("将发送 %d 条测试消息\n", 5);
    printf("=========================\n");
    
    for (int i = 0; test_messages[i] != NULL; i++) {
        printf("\n测试 %d: 发送 '%s'\n", i + 1, test_messages[i]);
        
        // 发送消息
        int ret = send_message_to_server(test_messages[i]);
        if (ret < 0) {
            printf("发送失败，终止测试\n");
            break;
        }
        
        // 接收响应
        ret = receive_server_response(response, sizeof(response));
        if (ret <= 0) {
            printf("接收失败，终止测试\n");
            break;
        }
        
        message_count++;
        
        // 等待一段时间
        sleep(1);
    }
    
    printf("\n测试完成，共发送 %d 条消息\n", message_count);
}

/**
 * @brief 连接测试模式
 */
int connection_test_mode(void)
{
    int max_retries = 3;
    int retry_delay = 2;  // 秒
    
    printf("\n=== 连接测试模式 ===\n");
    
    for (int attempt = 1; attempt <= max_retries; attempt++) {
        printf("连接尝试 %d/%d...\n", attempt, max_retries);
        
        int ret = connect_to_server(SERVER_IP, SERVER_PORT);
        if (ret == 0) {
            printf("连接成功\n");
            
            // 发送测试消息
            send_message_to_server("Connection test message");
            
            // 接收响应
            char response[BUFFER_SIZE];
            receive_server_response(response, sizeof(response));
            
            // 关闭连接
            qcm_socket_close(client_socket);
            client_socket = -1;
            
            printf("测试完成\n");
            return 0;
        }
        
        if (attempt < max_retries) {
            printf("连接失败，%d 秒后重试...\n", retry_delay);
            sleep(retry_delay);
            retry_delay *= 2;  // 指数退避
        }
    }
    
    printf("连接测试失败，达到最大重试次数\n");
    return -1;
}

/**
 * @brief SSL/TLS测试（如果启用）
 */
#ifdef CONFIG_QCM_VTLS_FUNC
void ssl_test_mode(void)
{
    printf("\n=== SSL/TLS测试模式 ===\n");
    
    // 注意：需要先连接服务器
    if (client_socket < 0) {
        printf("请先连接到服务器\n");
        return;
    }
    
    // 配置SSL
    qcm_ssl_config_t ssl_config;
    memset(&ssl_config, 0, sizeof(ssl_config));
    
    // 设置SSL参数（根据实际情况配置）
    // ssl_config.ca_cert = "ca.crt";
    // ssl_config.client_cert = "client.crt";
    // ssl_config.client_key = "client.key";
    
    printf("配置SSL参数...\n");
    int ret = qcm_socket_ssl_config(client_socket, &ssl_config, SERVER_IP, QOSA_TRUE);
    if (ret != 0) {
        printf("SSL配置失败，错误码: %d\n", ret);
        return;
    }
    
    printf("SSL配置成功，开始握手...\n");
    ret = qcm_socket_ssl_connect(client_socket);
    if (ret != 0) {
        printf("SSL握手失败，错误码: %d\n", ret);
        return;
    }
    
    printf("SSL连接成功！可以进行安全通信\n");
    
    // 测试SSL连接上的数据发送
    const char *secure_message = "Secure message over SSL";
    ret = qcm_socket_send(client_socket, (char *)secure_message, strlen(secure_message));
    if (ret > 0) {
        printf("SSL数据发送成功: %d 字节\n", ret);
    }
    
    // 清理SSL资源
    qcm_socket_ssl_clean(client_socket);
    printf("SSL资源已清理\n");
}
#endif

/**
 * @brief 显示菜单
 */
void show_menu(void)
{
    printf("\n====== TCP客户端菜单 ======\n");
    printf("1. 交互式聊天模式\n");
    printf("2. 自动测试模式\n");
    printf("3. 连接测试模式\n");
    printf("4. 获取Socket选项\n");
    printf("5. 设置Socket选项\n");
#ifdef CONFIG_QCM_VTLS_FUNC
    printf("6. SSL/TLS测试\n");
#endif
    printf("0. 退出\n");
    printf("============================\n");
    printf("请选择: ");
}

/**
 * @brief 获取Socket选项示例
 */
void get_socket_options(void)
{
    if (client_socket < 0) {
        printf("请先连接到服务器\n");
        return;
    }
    
    // 获取未读数据状态
    int unread_status = 0;
    int ret = qcm_socket_get_opt(client_socket, QCM_SOCK_UNREAD_OPT, &unread_status);
    if (ret == 0) {
        printf("未读数据状态: %d\n", unread_status);
    }
}

/**
 * @brief 设置Socket选项示例
 */
void set_socket_options(void)
{
    if (client_socket < 0) {
        printf("请先连接到服务器\n");
        return;
    }
    
    // 设置地址重用选项
    int reuse_addr = 1;
    int ret = qcm_socket_set_opt(client_socket, QCM_SOCK_REUSEADDR_OPT, &reuse_addr);
    if (ret == 0) {
        printf("地址重用选项设置成功\n");
    } else {
        printf("设置失败，错误码: %d\n", ret);
    }
    
    // 设置TCP延迟确认
    int delay_ack = 1;
    ret = qcm_socket_set_system_opt(QCM_TCP_DELAY_ACK_OPT, &delay_ack);
    if (ret == 0) {
        printf("TCP延迟确认设置成功\n");
    }
}

/**
 * @brief 主函数：TCP客户端入口
 */
int main(void)
{
    int choice;
    
    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("====== TCP客户端示例程序 ======\n");
    printf("服务器地址: %s:%d\n", SERVER_IP, SERVER_PORT);
    printf("================================\n");
    
    // 连接到服务器
    if (connect_to_server(SERVER_IP, SERVER_PORT) < 0) {
        printf("无法连接到服务器\n");
        return -1;
    }
    
    // 主菜单循环
    while (running) {
        show_menu();
        
        if (scanf("%d", &choice) != 1) {
            printf("输入无效\n");
            while (getchar() != '\n');  // 清空输入缓冲区
            continue;
        }
        
        switch (choice) {
            case 1:
                interactive_chat_mode();
                break;
            case 2:
                auto_test_mode();
                break;
            case 3:
                connection_test_mode();
                // 重新连接
                connect_to_server(SERVER_IP, SERVER_PORT);
                break;
            case 4:
                get_socket_options();
                break;
            case 5:
                set_socket_options();
                break;
#ifdef CONFIG_QCM_VTLS_FUNC
            case 6:
                ssl_test_mode();
                break;
#endif
            case 0:
                printf("正在退出...\n");
                running = 0;
                break;
            default:
                printf("无效选择\n");
                break;
        }
    }
    
    // 清理资源
    if (client_socket >= 0) {
        printf("正在关闭Socket连接...\n");
        int ret = qcm_socket_close(client_socket);
        if (ret < 0) {
            printf("关闭Socket失败，错误码: %d\n", ret);
        } else {
            printf("Socket连接已关闭\n");
        }
    }
    
    // 获取最后关闭原因
    if (client_socket >= 0) {
        int close_reason = qcm_socket_get_last_close_event(client_socket);
        printf("TCP连接关闭原因: %d\n", close_reason);
    }
    
    printf("TCP客户端示例程序结束\n");
    return 0;
}

/**
 * @brief 简单的命令行TCP客户端
 * 
 * 用法: ./tcp_client [server_ip] [server_port] [message]
 */
int simple_tcp_client(int argc, char *argv[])
{
    char *server_ip = SERVER_IP;
    int server_port = SERVER_PORT;
    char *message = "Hello from TCP Client";
    
    // 解析命令行参数
    if (argc >= 2) {
        server_ip = argv[1];
    }
    if (argc >= 3) {
        server_port = atoi(argv[2]);
    }
    if (argc >= 4) {
        message = argv[3];
    }
    
    printf("连接到服务器 %s:%d\n", server_ip, server_port);
    
    // 连接到服务器
    if (connect_to_server(server_ip, server_port) < 0) {
        return -1;
    }
    
    // 发送消息
    send_message_to_server(message);
    
    // 接收响应
    char buffer[BUFFER_SIZE];
    int ret = receive_server_response(buffer, sizeof(buffer));
    
    // 关闭连接
    qcm_socket_close(client_socket);
    
    return (ret > 0) ? 0 : -1;
}

// 编译指令示例
// gcc -o tcp_client tcp_client.c -I./include -L./lib -lqcm_socket -lpthread