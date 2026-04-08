#include "qosa_sys.h"
#include <stdio.h>

// 定义用户数据结构
typedef struct {
    int counter;
    char name[32];
} UserData;

// 定时器回调函数
void timer_callback(void* argv)
{
    UserData* data = (UserData*)argv;
    if (data) {
        data->counter++;
        printf("[定时器回调] %s: 第%d次触发\n", data->name, data->counter);
    }
}

int main(void)
{
    int ret;
    qosa_timer_t my_timer = NULL;
    UserData user_data = {0, "测试定时器"};
    
    // 1. 创建定时器
    ret = qosa_timer_create(&my_timer, timer_callback, &user_data);
    if (ret != QOSA_OK) {
        printf("定时器创建失败: %d\n", ret);
        return -1;
    }
    printf("定时器创建成功\n");
    
    // 2. 启动定时器（循环模式，每1000ms触发一次）
    ret = qosa_timer_start(my_timer, 1000, QOSA_TRUE);
    if (ret != QOSA_OK) {
        printf("定时器启动失败: %d\n", ret);
        qosa_timer_delete(my_timer);
        return -1;
    }
    printf("定时器启动成功，模式：循环，间隔：1000ms\n");
    
    // 3. 检查定时器状态
    if (qosa_timer_is_running(my_timer)) {
        printf("定时器正在运行\n");
    }
    
    // 4. 模拟运行一段时间（实际应用中可能等待事件或休眠）
    for (int i = 0; i < 10; i++) {
        qosa_sleep(500);  // 休眠500ms
        printf("主程序运行中...\n");
    }
    
    // 5. 停止定时器
    ret = qosa_timer_stop(my_timer);
    if (ret == QOSA_OK) {
        printf("定时器已停止\n");
    }
    
    // 6. 删除定时器
    ret = qosa_timer_delete(my_timer);
    if (ret == QOSA_OK) {
        printf("定时器已删除\n");
    }
    
    return 0;
}