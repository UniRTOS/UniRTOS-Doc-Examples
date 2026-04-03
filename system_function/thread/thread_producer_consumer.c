#include "qosa_sys.h"
#include <stdio.h>

typedef struct {
    int sensor_id;
    float value;
} SensorData;

void producer_task(void *argv)
{
    qosa_task_t consumer = *(qosa_task_t*)argv;  // 获取消费者任务句柄
    SensorData data = {1, 0.0};
    int seq = 0;
    while (1) {
        data.value = (float)seq++;
        qosa_task_msg_release(consumer, sizeof(SensorData), (uint8_t*)&data, QOSA_NO_WAIT);
        printf("[P] sent: id=%d, val=%.1f\n", data.sensor_id, data.value);
        qosa_task_sleep_ms(500);
    }
}

void consumer_task(void *argv)
{
    SensorData data;
    while (1) {
        if (qosa_task_msg_wait(NULL, (uint8_t*)&data, sizeof(SensorData), QOSA_WAIT_FOREVER) == 0) {
            printf("[C] recv: id=%d, val=%.1f\n", data.sensor_id, data.value);
        }
        qosa_task_sleep_ms(10);  // 模拟处理耗时
    }
}

int main(void)
{
    qosa_task_t consumer, producer;

    qosa_task_create_ex(&consumer, 2048, 8, "consumer", consumer_task, NULL, 10, sizeof(SensorData));
    qosa_task_create(&producer, 1024, 6, "producer", producer_task, &consumer);

    while (1) qosa_task_sleep_ms(1000);
}