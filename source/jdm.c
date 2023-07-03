#include "../include/jdm.h"
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

struct jdm_message
{
    jdm_message_level level;
    const char* file;
    const char* function;
    uint32_t line;
    char message[];
};

typedef struct jdm_state_struct jdm_state;
struct jdm_state_struct
{
    int32_t init;

    char* thrd_name;
    size_t len_name;

    uint32_t error_count;
    uint32_t error_capacity;
    struct jdm_message** errors;

    uint32_t stacktrace_count;
    uint32_t stacktrace_capacity;
    const char** stack_traces;

    jdm_message_level level;

    jdm_error_hook_fn hook;
    void* hook_param;

    jallocator* allocator;
};

_Thread_local jdm_state JDM_THREAD_ERROR_STATE;

int jdm_error_init_thread(
        char* thread_name, jdm_message_level level, uint32_t max_stack_trace, uint32_t max_errors, jallocator* allocator)
{
    assert(JDM_THREAD_ERROR_STATE.init == 0);
    JDM_THREAD_ERROR_STATE.len_name = thread_name ? strlen(thread_name) : 0;
    char* name = NULL;
    if (JDM_THREAD_ERROR_STATE.len_name)
    {
        name = jalloc(allocator, JDM_THREAD_ERROR_STATE.len_name + 1);
        if (!name)
        {
            return -1;
        }
        memcpy(name, thread_name, JDM_THREAD_ERROR_STATE.len_name);
        name[JDM_THREAD_ERROR_STATE.len_name] = 0;
    }
    JDM_THREAD_ERROR_STATE.thrd_name = name;
    assert(JDM_THREAD_ERROR_STATE.thrd_name != NULL);
    JDM_THREAD_ERROR_STATE.level = level;
    JDM_THREAD_ERROR_STATE.errors = jalloc(allocator, max_errors * sizeof(*JDM_THREAD_ERROR_STATE.errors));;
    if (!JDM_THREAD_ERROR_STATE.errors)
    {
        jfree(allocator, JDM_THREAD_ERROR_STATE.thrd_name);
        return -1;
    }
    assert(JDM_THREAD_ERROR_STATE.errors != NULL);
    JDM_THREAD_ERROR_STATE.error_capacity = max_errors;

    JDM_THREAD_ERROR_STATE.stack_traces = jalloc(allocator, max_stack_trace * sizeof(char*));
    if (!JDM_THREAD_ERROR_STATE.stack_traces)
    {
        jfree(allocator, JDM_THREAD_ERROR_STATE.errors);
        jfree(allocator, JDM_THREAD_ERROR_STATE.thrd_name);
        return -1;
    }
    assert(JDM_THREAD_ERROR_STATE.stack_traces != NULL);
    JDM_THREAD_ERROR_STATE.stacktrace_capacity = max_stack_trace;
    JDM_THREAD_ERROR_STATE.allocator = allocator;
    return 0;
}

void jdm_error_cleanup_thread(void)
{
    assert(JDM_THREAD_ERROR_STATE.stacktrace_count == 0);
    jfree(JDM_THREAD_ERROR_STATE.allocator, JDM_THREAD_ERROR_STATE.thrd_name);
    for (uint32_t i = 0; i < JDM_THREAD_ERROR_STATE.error_count; ++i)
    {
        jfree(JDM_THREAD_ERROR_STATE.allocator, JDM_THREAD_ERROR_STATE.errors[i]);
    }
    jfree(JDM_THREAD_ERROR_STATE.allocator, JDM_THREAD_ERROR_STATE.errors);
    jfree(JDM_THREAD_ERROR_STATE.allocator, JDM_THREAD_ERROR_STATE.stack_traces);
    memset(&JDM_THREAD_ERROR_STATE, 0, sizeof(JDM_THREAD_ERROR_STATE));
}

uint32_t jdm_error_enter_function(const char* fn_name)
{
    uint32_t id = JDM_THREAD_ERROR_STATE.stacktrace_count;
    if (JDM_THREAD_ERROR_STATE.stacktrace_count < JDM_THREAD_ERROR_STATE.stacktrace_capacity)
    {
        JDM_THREAD_ERROR_STATE.stack_traces[JDM_THREAD_ERROR_STATE.stacktrace_count++] = fn_name;
    }
    return id;
}

void jdm_error_leave_function(const char* fn_name, uint32_t level)
{
    if (level < JDM_THREAD_ERROR_STATE.stacktrace_capacity)
    {
        assert(JDM_THREAD_ERROR_STATE.stacktrace_count == level + 1);
        assert(JDM_THREAD_ERROR_STATE.stack_traces[level] == fn_name);
        JDM_THREAD_ERROR_STATE.stacktrace_count = level;
    }
    else
    {
        assert(level == JDM_THREAD_ERROR_STATE.stacktrace_capacity);
        assert(JDM_THREAD_ERROR_STATE.stacktrace_count == JDM_THREAD_ERROR_STATE.stacktrace_capacity);
    }
}

void jdm_error_push(jdm_message_level level, uint32_t line, const char* file, const char* function, const char* fmt, ...)
{
    if (level < JDM_THREAD_ERROR_STATE.level || JDM_THREAD_ERROR_STATE.error_count == JDM_THREAD_ERROR_STATE.error_capacity) return;
    va_list args1, args2;
    va_start(args1, fmt);
    va_copy(args2, args1);
    const size_t error_length = JDM_THREAD_ERROR_STATE.len_name + vsnprintf(NULL, 0, fmt, args1) + 4;
    va_end(args1);
    struct jdm_message* message = jalloc(JDM_THREAD_ERROR_STATE.allocator, sizeof(*message) + error_length);
    assert(message);
    size_t used = vsnprintf(message->message, error_length, fmt, args2);
    va_end(args2);
    snprintf(message->message + used, error_length - used, " (%s)", JDM_THREAD_ERROR_STATE.thrd_name);
    int32_t put_in_stack = 1;
    if (JDM_THREAD_ERROR_STATE.hook)
    {
        put_in_stack = JDM_THREAD_ERROR_STATE.hook(JDM_THREAD_ERROR_STATE.thrd_name, JDM_THREAD_ERROR_STATE.stacktrace_count, JDM_THREAD_ERROR_STATE.stack_traces, level, line, file, function, message->message, JDM_THREAD_ERROR_STATE.hook_param);
    }

    if (put_in_stack)
    {
        message->file = file;
        message->level = level;
        message->function = function;
        message->line = line;
        JDM_THREAD_ERROR_STATE.errors[JDM_THREAD_ERROR_STATE.error_count++] = message;
    }
    else
    {
        jfree(JDM_THREAD_ERROR_STATE.allocator, message);
    }
}

void jdm_error_report_critical(const char* fmt, ...)
{
    fprintf(stderr, "Critical error:\n");
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\t(%s : %s", JDM_THREAD_ERROR_STATE.thrd_name, JDM_THREAD_ERROR_STATE.stack_traces[0]);
    for (uint32_t i = 1; i < JDM_THREAD_ERROR_STATE.stacktrace_count; ++i)
    {
        fprintf(stderr, " -> %s", JDM_THREAD_ERROR_STATE.stack_traces[i]);
    }
    fprintf(stderr, ")\nTerminating program\n");
    exit(EXIT_FAILURE);
}

void jdm_error_process(jdm_error_report_fn function, void* param)
{
    assert(function);
    uint32_t i;
    for (i = 0; i < JDM_THREAD_ERROR_STATE.error_count; ++i)
    {
        struct jdm_message* msg = JDM_THREAD_ERROR_STATE.errors[JDM_THREAD_ERROR_STATE.error_count - 1 - i];
        const int32_t ret = function(JDM_THREAD_ERROR_STATE.error_count, i, msg->level, msg->line, msg->file, msg->function, msg->message, param);
        if (ret < 0) break;
    }
    for (uint32_t j = 0; j < i; ++j)
    {
        struct jdm_message* msg = JDM_THREAD_ERROR_STATE.errors[JDM_THREAD_ERROR_STATE.error_count - 1 - j];
        JDM_THREAD_ERROR_STATE.errors[JDM_THREAD_ERROR_STATE.error_count - 1 - j] = NULL;
        jfree(JDM_THREAD_ERROR_STATE.allocator, msg);
    }
    JDM_THREAD_ERROR_STATE.error_count = 0;
}

void jdm_error_peek(jdm_error_report_fn function, void* param)
{
    assert(function);
    uint32_t i;
    for (i = 0; i < JDM_THREAD_ERROR_STATE.error_count; ++i)
    {
        struct jdm_message* msg = JDM_THREAD_ERROR_STATE.errors[JDM_THREAD_ERROR_STATE.error_count - 1 - i];
        const int32_t ret = function(JDM_THREAD_ERROR_STATE.error_count, JDM_THREAD_ERROR_STATE.error_count - 1 - i, msg->level, msg->line, msg->file, msg->function, msg->message, param);
        if (ret < 0) break;
    }
}

void jdm_error_set_hook(jdm_error_hook_fn function, void* param)
{
    JDM_THREAD_ERROR_STATE.hook = function;
    JDM_THREAD_ERROR_STATE.hook_param = param;
}

const char* jdm_message_level_str(jdm_message_level level)
{
    static const char* const NAMES[] =
            {
                    [JDM_ERROR_LEVEL_NONE] = "JDM_ERROR_LEVEL_NONE" ,
                    [JDM_ERROR_LEVEL_INFO] = "JDM_ERROR_LEVEL_INFO" ,
                    [JDM_ERROR_LEVEL_WARN] = "JDM_ERROR_LEVEL_WARN" ,
                    [JDM_ERROR_LEVEL_ERR] = "JDM_ERROR_LEVEL_ERR"  ,
                    [JDM_ERROR_LEVEL_CRIT] = "JDM_ERROR_LEVEL_CRIT" ,
            };
    if (level >= JDM_ERROR_LEVEL_NONE && level <= JDM_ERROR_LEVEL_CRIT)
    {
        return NAMES[level];
    }
    return NULL;
}
