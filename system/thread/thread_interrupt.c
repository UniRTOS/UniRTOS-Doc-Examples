#include "qosa_sys.h"
#include <stdio.h>

// 假设这是一个外部中断的ISR（简化示例）
void EXTI_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t raw_data = 0x1234;  // 从硬件读取的数据

    // 将数据发送给处理任务
    extern qosa_task_t g_irq_handler_task;
    qosa_task_msg_release(g_irq_handler_task, sizeof(raw_data), (uint8_t*)&raw_data, QOSA_NO_WAIT);

    // 标记是否需要任务切换（RTOS相关）
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// 专门处理中断数据的任务
void irq_handler_task(void *argv)
{
    uint32_t data;
    while (1) {
        if (qosa_task_msg_wait(NULL, (uint8_t*)&data, sizeof(data), QOSA_WAIT_FOREVER) == 0) {
            printf("IRQ data: 0x%X, processing...\n", data);
            // 进行耗时数据处理
            qosa_task_sleep_ms(10);
        }
    }
}

qosa_task_t g_irq_handler_task;

int main(void)
{
    // 创建带消息队列的处理任务
    qosa_task_create_ex(&g_irq_handler_task, 2048, 15, "irq_hdlr", irq_handler_task, NULL, 20, sizeof(uint32_t));

    // 配置中断、注册ISR等（略）
    while (1) qosa_task_sleep_ms(1000);
}