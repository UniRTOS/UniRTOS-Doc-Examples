#include "qosa_sys.h"
#include "qosa_def.h"
#include "qosa_log.h"

#define QOS_LOG_TAG LOG_TAG_DEMO
#define MEMORY_DEMO_TASK_STACK_SIZE 4096
#define MEMORY_DEMO_BUFFER_SIZE 64
#define MEMORY_DEMO_REALLOC_SIZE 128
#define MEMORY_DEMO_ARRAY_COUNT 8

static qosa_task_t g_unir_memory_demo_task = QOSA_NULL;

static qosa_bool_t memory_demo_malloc_free(void)
{
	char *buffer = QOSA_NULL;

	QLOGI("1. qosa_malloc/qosa_free demo start");

	buffer = (char *)qosa_malloc(MEMORY_DEMO_BUFFER_SIZE);
	if (buffer == QOSA_NULL)
	{
		QLOGE("qosa_malloc failed, size=%lu", (unsigned long)MEMORY_DEMO_BUFFER_SIZE);
		return QOSA_FALSE;
	}

	qosa_memset(buffer, 0, MEMORY_DEMO_BUFFER_SIZE);
	qosa_snprintf(buffer, MEMORY_DEMO_BUFFER_SIZE, "%s", "hello qosa malloc");
	QLOGI("malloc buffer: %s", buffer);

	qosa_free(buffer);
	buffer = QOSA_NULL;

	QLOGI("1. qosa_malloc/qosa_free demo end");
	return QOSA_TRUE;
}

static qosa_bool_t memory_demo_calloc_free(void)
{
	qosa_uint32_t *array = QOSA_NULL;
	qosa_uint32_t  index = 0;

	QLOGI("2. qosa_calloc/qosa_free demo start");

	array = (qosa_uint32_t *)qosa_calloc(MEMORY_DEMO_ARRAY_COUNT, sizeof(qosa_uint32_t));
	if (array == QOSA_NULL)
	{
		QLOGE("qosa_calloc failed, count=%lu, size=%lu", (unsigned long)MEMORY_DEMO_ARRAY_COUNT, (unsigned long)sizeof(qosa_uint32_t));
		return QOSA_FALSE;
	}

	QLOGI("calloc default value: array[0]=%lu, array[%lu]=%lu",
		  (unsigned long)array[0],
		  (unsigned long)(MEMORY_DEMO_ARRAY_COUNT - 1),
		  (unsigned long)array[MEMORY_DEMO_ARRAY_COUNT - 1]);

	for (index = 0; index < MEMORY_DEMO_ARRAY_COUNT; index++)
	{
		array[index] = index + 1;
	}

	QLOGI("calloc assigned value: array[0]=%lu, array[%lu]=%lu",
		  (unsigned long)array[0],
		  (unsigned long)(MEMORY_DEMO_ARRAY_COUNT - 1),
		  (unsigned long)array[MEMORY_DEMO_ARRAY_COUNT - 1]);

	qosa_free(array);
	array = QOSA_NULL;

	QLOGI("2. qosa_calloc/qosa_free demo end");
	return QOSA_TRUE;
}

static qosa_bool_t memory_demo_realloc_free(void)
{
	char *buffer = QOSA_NULL;
	char *new_buffer = QOSA_NULL;

	QLOGI("3. qosa_realloc/qosa_free demo start");

	buffer = (char *)qosa_malloc(MEMORY_DEMO_BUFFER_SIZE);
	if (buffer == QOSA_NULL)
	{
		QLOGE("qosa_malloc failed before realloc");
		return QOSA_FALSE;
	}

	qosa_memset(buffer, 0, MEMORY_DEMO_BUFFER_SIZE);
	qosa_snprintf(buffer, MEMORY_DEMO_BUFFER_SIZE, "%s", "before realloc");
	QLOGI("original buffer: %s", buffer);

	new_buffer = (char *)qosa_realloc(buffer, MEMORY_DEMO_REALLOC_SIZE);
	if (new_buffer == QOSA_NULL)
	{
		QLOGE("qosa_realloc failed, keep and free original buffer");
		qosa_free(buffer);
		return QOSA_FALSE;
	}

	buffer = new_buffer;
	qosa_snprintf(buffer + qosa_strlen(buffer), MEMORY_DEMO_REALLOC_SIZE - qosa_strlen(buffer), "%s", ", after realloc");
	QLOGI("realloc buffer: %s", buffer);

	qosa_free(buffer);
	buffer = QOSA_NULL;

	QLOGI("3. qosa_realloc/qosa_free demo end");
	return QOSA_TRUE;
}

static qosa_bool_t memory_demo_strdup_free(void)
{
	char *string_copy = QOSA_NULL;
	const char *source_string = "hello qosa strdup";

	QLOGI("4. qosa_strdup/qosa_free demo start");

	string_copy = qosa_strdup(source_string);
	if (string_copy == QOSA_NULL)
	{
		QLOGE("qosa_strdup failed");
		return QOSA_FALSE;
	}

	QLOGI("source string: %s", source_string);
	QLOGI("heap string copy: %s, length=%lu", string_copy, (unsigned long)qosa_strlen(string_copy));

	qosa_free(string_copy);
	string_copy = QOSA_NULL;

	QLOGI("4. qosa_strdup/qosa_free demo end");
	return QOSA_TRUE;
}

static void unir_memory_demo_process(void *ctx)
{
	(void)ctx;

	qosa_task_sleep_sec(3);

	QLOGI("===== memory management demo start =====");

	if (memory_demo_malloc_free() != QOSA_TRUE)
	{
		QLOGE("malloc/free demo failed");
	}

	if (memory_demo_calloc_free() != QOSA_TRUE)
	{
		QLOGE("calloc/free demo failed");
	}

	if (memory_demo_realloc_free() != QOSA_TRUE)
	{
		QLOGE("realloc/free demo failed");
	}

	if (memory_demo_strdup_free() != QOSA_TRUE)
	{
		QLOGE("strdup/free demo failed");
	}

	QLOGI("===== memory management demo end =====");
}

void unir_memory_demo_init(void)
{
	QLOGI("enter UniRTOS memory management demo");

	if (g_unir_memory_demo_task != QOSA_NULL)
	{
		QLOGI("memory demo task already created");
		return;
	}

	if (qosa_task_create(&g_unir_memory_demo_task,
						 MEMORY_DEMO_TASK_STACK_SIZE,
						 QOSA_PRIORITY_NORMAL,
						 "memory_demo",
						 unir_memory_demo_process,
						 QOSA_NULL) != QOSA_OK)
	{
		QLOGE("create memory demo task failed");
	}
}
