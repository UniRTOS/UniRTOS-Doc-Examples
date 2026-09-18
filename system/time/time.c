#include "qosa_sys.h"
#include "qosa_rtc.h"
#include "qosa_def.h"
#include "qosa_log.h"

#define QOS_LOG_TAG LOG_TAG_DEMO
#define SYSTEM_TIME_DEMO_TASK_STACK_SIZE 4096

static qosa_task_t g_system_time_demo_task = QOSA_NULL;

static void system_time_demo_print_current_time(void)
{
	qosa_bool_t      ret = QOSA_FALSE;
	qosa_time_info_t time_info = {0};
	qosa_time_t      seconds = 0;
	qosa_time_t      milliseconds = 0;
	qosa_time_t      microseconds = 0;

	ret = qosa_get_system_time(&time_info);
	if (ret != QOSA_TRUE)
	{
		QLOGE("qosa_get_system_time failed");
		return;
	}

	seconds = qosa_get_system_time_seconds();
	milliseconds = qosa_get_system_time_milliseconds();
	microseconds = qosa_get_system_time_microseconds();

	QLOGI("system time info: seconds=%llu, microseconds=%llu", (unsigned long long)time_info.seconds, (unsigned long long)time_info.microseconds);
	QLOGI("time from 1970-01-01: %llu s, %llu ms, %llu us", (unsigned long long)seconds, (unsigned long long)milliseconds, (unsigned long long)microseconds);
}

static void system_time_demo_process(void *ctx)
{
	qosa_uint32_t start_tick = 0;
	qosa_uint32_t end_tick = 0;
	qosa_time_t   start_ms = 0;
	qosa_time_t   end_ms = 0;

	(void)ctx;

	qosa_task_sleep_sec(3);

	QLOGI("===== system time demo start =====");

	QLOGI("current system tick count: %lu", (unsigned long)qosa_get_system_tick_cnt());
	system_time_demo_print_current_time();

	start_tick = qosa_get_system_tick_cnt();
	start_ms = qosa_get_system_time_milliseconds();
	qosa_task_sleep_ms(1000);
	end_tick = qosa_get_system_tick_cnt();
	end_ms = qosa_get_system_time_milliseconds();

	QLOGI("after sleep 1000 ms, tick delta=%lu, time delta=%llu ms", (unsigned long)(end_tick - start_tick), (unsigned long long)(end_ms - start_ms));

	system_time_demo_print_current_time();

	QLOGI("===== system time demo end =====");
}

void system_time_demo_init(void)
{
	QLOGI("enter system time demo");

	if (g_system_time_demo_task != QOSA_NULL)
	{
		QLOGI("system time demo task already created");
		return;
	}

	if (qosa_task_create(&g_system_time_demo_task,
						 SYSTEM_TIME_DEMO_TASK_STACK_SIZE,
						 QOSA_PRIORITY_NORMAL,
						 "sys_time_demo",
						 system_time_demo_process,
						 QOSA_NULL) != QOSA_OK)
	{
		QLOGE("create system time demo task failed");
	}
}
