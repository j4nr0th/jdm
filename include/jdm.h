#ifndef JDM_JDM_H
#define JDM_JDM_H
#include <errno.h>
#include <stdint.h>
#include <jmem/jmem.h>

typedef enum jdm_error_level_enum jdm_error_level;
enum jdm_error_level_enum
{
    JDM_ERROR_LEVEL_NONE = 0,
    JDM_ERROR_LEVEL_INFO = 1,
    JDM_ERROR_LEVEL_WARN = 2,
    JDM_ERROR_LEVEL_ERR = 3,
    JDM_ERROR_LEVEL_CRIT = 4,
};

const char* jdm_error_level_str(jdm_error_level level);

int jdm_error_init_thread(
        char* thread_name, jdm_error_level level, uint32_t max_stack_trace, uint32_t max_errors, jallocator* allocator);
void jdm_error_cleanup_thread(void);

uint32_t jdm_error_enter_function(const char* fn_name);
void jdm_error_leave_function(const char* fn_name, uint32_t level);

void jdm_error_push(jdm_error_level level, uint32_t line, const char* file, const char* function, const char* fmt, ...);
void jdm_error_report_critical(const char* fmt, ...);

typedef int (*jdm_error_report_fn)(uint32_t total_count, uint32_t index, jdm_error_level level, uint32_t line, const char* file, const char* function, const char* message, void* param);
typedef int (*jdm_error_hook_fn)(const char* thread_name, uint32_t stack_trace_count, const char*const* stack_trace, jdm_error_level level, uint32_t line, const char* file, const char* function, const char* message, void* param);

void jdm_error_process(jdm_error_report_fn function, void* param);
void jdm_error_peek(jdm_error_report_fn function, void* param);
void jdm_error_set_hook(jdm_error_hook_fn function, void* param);

#define JDM_ERRNO_MESSAGE strerror(errno)
#define JDM_ERROR(fmt, ...) jdm_error_push(JDM_ERROR_LEVEL_ERR, __LINE__, __FILE__, __func__, fmt __VA_OPT__(,) __VA_ARGS__)
#define JDM_WARN(fmt, ...) jdm_error_push(JDM_ERROR_LEVEL_WARN, __LINE__, __FILE__, __func__, fmt __VA_OPT__(,) __VA_ARGS__)
#define JDM_INFO(fmt, ...) jdm_error_push(JDM_ERROR_LEVEL_INFO, __LINE__, __FILE__, __func__, fmt __VA_OPT__(,) __VA_ARGS__)
#define JDM_ERROR_CRIT(fmt, ...) jdm_error_report_critical("JFW Error from \"%s\" at line %i: " fmt "\n", __FILE__, __LINE__ __VA_OPT__(,) __VA_ARGS__)
#ifndef NDEBUG
#define JDM_ENTER_FUNCTION const u32 _jdm_stacktrace_level = jdm_error_enter_function(__func__)
#define JDM_LEAVE_FUNCTION jdm_error_leave_function(__func__, _jdm_stacktrace_level)
#else
#define JDM_ENTER_FUNCTION (void)0
#define JDM_LEAVE_FUNCTION (void)0
#endif
#endif //JDM_JDM_H
