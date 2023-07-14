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

    jdm_allocator_callbacks allocator_callbacks;
};

_Thread_local jdm_state JDM_THREAD_ERROR_STATE;

static void* wrap_alloc(void* param, uint64_t size)
{
    (void)param;
    return malloc(size);
}

static void wrap_free(void* param, void* ptr)
{
    (void)param;
    free(ptr);
}

static const jdm_allocator_callbacks DEFAULT_CALLBACKS =
        {
            .alloc = wrap_alloc,
            .free = wrap_free,
            .param = NULL,
        };

int jdm_init_thread(
        const char* thread_name, jdm_message_level level, uint32_t max_stack_trace, uint32_t max_errors,
        const jdm_allocator_callbacks* allocator_callbacks)
{
    if (!allocator_callbacks)
    {
        allocator_callbacks = &DEFAULT_CALLBACKS;
    }
    JDM_THREAD_ERROR_STATE.len_name = thread_name ? strlen(thread_name) : 0;
    char* name = NULL;
    if (JDM_THREAD_ERROR_STATE.len_name)
    {
        name = allocator_callbacks->alloc(allocator_callbacks->param, JDM_THREAD_ERROR_STATE.len_name + 1);
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
    JDM_THREAD_ERROR_STATE.errors = allocator_callbacks->alloc(allocator_callbacks->param, max_errors * sizeof(*JDM_THREAD_ERROR_STATE.errors));;
    if (!JDM_THREAD_ERROR_STATE.errors)
    {
        allocator_callbacks->free(allocator_callbacks->param, JDM_THREAD_ERROR_STATE.thrd_name);
        return -1;
    }
    assert(JDM_THREAD_ERROR_STATE.errors != NULL);
    JDM_THREAD_ERROR_STATE.error_capacity = max_errors;

    JDM_THREAD_ERROR_STATE.stack_traces = allocator_callbacks->alloc(allocator_callbacks->param, max_stack_trace * sizeof(char*));
    if (!JDM_THREAD_ERROR_STATE.stack_traces)
    {
        allocator_callbacks->free(allocator_callbacks->param, JDM_THREAD_ERROR_STATE.errors);
        allocator_callbacks->free(allocator_callbacks->param, JDM_THREAD_ERROR_STATE.thrd_name);
        return -1;
    }
    assert(JDM_THREAD_ERROR_STATE.stack_traces != NULL);
    JDM_THREAD_ERROR_STATE.stacktrace_capacity = max_stack_trace;
    JDM_THREAD_ERROR_STATE.allocator_callbacks = *allocator_callbacks;
    return 0;
}

void jdm_cleanup_thread(void)
{
    assert(JDM_THREAD_ERROR_STATE.stacktrace_count == 0);
    JDM_THREAD_ERROR_STATE.allocator_callbacks.free(JDM_THREAD_ERROR_STATE.allocator_callbacks.param, JDM_THREAD_ERROR_STATE.thrd_name);
    for (uint32_t i = 0; i < JDM_THREAD_ERROR_STATE.error_count; ++i)
    {
        JDM_THREAD_ERROR_STATE.allocator_callbacks.free(JDM_THREAD_ERROR_STATE.allocator_callbacks.param, JDM_THREAD_ERROR_STATE.errors[i]);
    }
    JDM_THREAD_ERROR_STATE.allocator_callbacks.free(JDM_THREAD_ERROR_STATE.allocator_callbacks.param, JDM_THREAD_ERROR_STATE.errors);
    JDM_THREAD_ERROR_STATE.allocator_callbacks.free(JDM_THREAD_ERROR_STATE.allocator_callbacks.param, JDM_THREAD_ERROR_STATE.stack_traces);
    memset(&JDM_THREAD_ERROR_STATE, 0, sizeof(JDM_THREAD_ERROR_STATE));
}

void jdm_get_stacktrace(const char* const** stack_trace_out, uint32_t* stack_trace_count_out){
    *stack_trace_count_out = JDM_THREAD_ERROR_STATE.stacktrace_count;
    *stack_trace_out = JDM_THREAD_ERROR_STATE.stack_traces;
}

const char* jdm_get_thread_name(void){
    return JDM_THREAD_ERROR_STATE.thrd_name;
}

uint32_t jdm_enter_function(const char* fn_name)
{
    uint32_t id = JDM_THREAD_ERROR_STATE.stacktrace_count;
    if (JDM_THREAD_ERROR_STATE.stacktrace_count < JDM_THREAD_ERROR_STATE.stacktrace_capacity)
    {
        JDM_THREAD_ERROR_STATE.stack_traces[JDM_THREAD_ERROR_STATE.stacktrace_count++] = fn_name;
    }
    return id;
}

void jdm_leave_function(const char* fn_name, uint32_t level)
{
    if (level < JDM_THREAD_ERROR_STATE.stacktrace_capacity)
    {
        assert(JDM_THREAD_ERROR_STATE.stacktrace_count == level + 1);
        assert(JDM_THREAD_ERROR_STATE.stack_traces[level] == fn_name);
        (void)fn_name;
        JDM_THREAD_ERROR_STATE.stacktrace_count = level;
    }
    else
    {
        assert(level == JDM_THREAD_ERROR_STATE.stacktrace_capacity);
        assert(JDM_THREAD_ERROR_STATE.stacktrace_count == JDM_THREAD_ERROR_STATE.stacktrace_capacity);
    }
}

void jdm_push_va(
        jdm_message_level level, uint32_t line, const char* file, const char* function, const char* fmt, va_list args)
{
    if (level < JDM_THREAD_ERROR_STATE.level || JDM_THREAD_ERROR_STATE.error_count == JDM_THREAD_ERROR_STATE.error_capacity) return;
    va_list args1;
    va_copy(args1, args);
    const size_t error_length = vsnprintf(NULL, 0, fmt, args1) + 1;
    va_end(args1);
    struct jdm_message* message = JDM_THREAD_ERROR_STATE.allocator_callbacks.alloc(JDM_THREAD_ERROR_STATE.allocator_callbacks.param, sizeof(*message) + error_length);
    assert(message);
    size_t used = vsnprintf(message->message, error_length, fmt, args);
    (void)used;
    assert(used + 1 == error_length);
    va_end(args);
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
        JDM_THREAD_ERROR_STATE.allocator_callbacks.free(JDM_THREAD_ERROR_STATE.allocator_callbacks.param, message);
    }
}

void jdm_push(jdm_message_level level, uint32_t line, const char* file, const char* function, const char* fmt, ...)
{
    if (level < JDM_THREAD_ERROR_STATE.level || JDM_THREAD_ERROR_STATE.error_count == JDM_THREAD_ERROR_STATE.error_capacity) return;
    va_list args;
    va_start(args, fmt);
    jdm_push_va(level, line, file, function, fmt, args);
    va_end(args);
}

void jdm_report_fatal_va(uint32_t line, const char* file, const char* function, const char* fmt, va_list args)
{
    size_t len = vsnprintf(NULL, 0, fmt, args) + 1;
    char msg[len];
    vsnprintf(msg, len, fmt, args);
    if (JDM_THREAD_ERROR_STATE.hook){
        JDM_THREAD_ERROR_STATE.hook(JDM_THREAD_ERROR_STATE.thrd_name, JDM_THREAD_ERROR_STATE.stacktrace_count, JDM_THREAD_ERROR_STATE.stack_traces, JDM_MESSAGE_LEVEL_FATAL, line, file, function, msg, JDM_THREAD_ERROR_STATE.hook_param);
    }else{
        fprintf(stderr, "Critical error:\n%s", msg);
        fprintf(stderr, "\t(%s : %s", JDM_THREAD_ERROR_STATE.thrd_name, JDM_THREAD_ERROR_STATE.stack_traces[0]);
        for (uint32_t i = 1; i < JDM_THREAD_ERROR_STATE.stacktrace_count; ++i)
        {
            fprintf(stderr, " -> %s", JDM_THREAD_ERROR_STATE.stack_traces[i]);
        }
        fprintf(stderr, ")\nTerminating program\n");
    }
    exit(EXIT_FAILURE);
}

void jdm_report_fatal(uint32_t line, const char* file, const char* function, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    jdm_report_fatal_va(line, file, function, fmt, args);
}

void jdm_process(jdm_error_report_fn function, void* param)
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
        JDM_THREAD_ERROR_STATE.allocator_callbacks.free(JDM_THREAD_ERROR_STATE.allocator_callbacks.param, msg);
    }
    JDM_THREAD_ERROR_STATE.error_count = 0;
}

void jdm_peek(jdm_error_report_fn function, void* param)
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

void jdm_set_hook(jdm_error_hook_fn function, void* param)
{
    JDM_THREAD_ERROR_STATE.hook = function;
    JDM_THREAD_ERROR_STATE.hook_param = param;
}

const char* jdm_message_level_str(jdm_message_level level)
{
    static const char* const NAMES[] =
            {

                    [JDM_MESSAGE_LEVEL_NONE] = "None",
                    [JDM_MESSAGE_LEVEL_DEBUG] = "Debug",
                    [JDM_MESSAGE_LEVEL_TRACE] = "Trace",
                    [JDM_MESSAGE_LEVEL_INFO] = "Info",
                    [JDM_MESSAGE_LEVEL_WARN] = "Warn",
                    [JDM_MESSAGE_LEVEL_ERR] = "Error",
                    [JDM_MESSAGE_LEVEL_CRIT] = "Critical",
                    [JDM_MESSAGE_LEVEL_FATAL] = "Fatal",
            };
    if (level >= JDM_MESSAGE_LEVEL_NONE && level <= JDM_MESSAGE_LEVEL_FATAL)
    {
        return NAMES[level];
    }
    return NULL;
}

void jdm_set_message_level(jdm_message_level level)
{
    JDM_THREAD_ERROR_STATE.level = level;
}
