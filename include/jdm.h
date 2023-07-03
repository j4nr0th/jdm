#ifndef JDM_JDM_H
#define JDM_JDM_H
#include <errno.h>
#include <stdint.h>
#include <jmem/jmem.h>

#ifdef __GNUC__
#define GCC_ONLY(x) x
#else
#define GCC_ONLY(x)
#endif

typedef enum jdm_message_level_enum jdm_message_level;
enum jdm_message_level_enum
{
    JDM_ERROR_LEVEL_NONE = 0,
    JDM_ERROR_LEVEL_DEBUG,
    JDM_ERROR_LEVEL_TRACE,
    JDM_ERROR_LEVEL_INFO,
    JDM_ERROR_LEVEL_WARN,
    JDM_ERROR_LEVEL_ERR,
    JDM_ERROR_LEVEL_CRIT,
};

GCC_ONLY(__attribute__((warn_unused_result))) const char* jdm_message_level_str(jdm_message_level level);

GCC_ONLY(__attribute__((nonnull(5)))) int jdm_init_thread(char* thread_name, jdm_message_level level, uint32_t max_stack_trace, uint32_t max_errors, jallocator* allocator);
void jdm_cleanup_thread(void);

GCC_ONLY(__attribute__((nonnull(1)))) uint32_t jdm_enter_function(const char* fn_name);
GCC_ONLY(__attribute__((nonnull(1)))) void jdm_leave_function(const char* fn_name, uint32_t level);

GCC_ONLY(__attribute__((format(printf, 5, 6)))) void jdm_push(jdm_message_level level, uint32_t line, const char* file, const char* function, const char* fmt, ...);
GCC_ONLY(__attribute__((format(printf, 1, 2)))) void jdm_report_fatal(const char* fmt, ...);

typedef int (*jdm_error_report_fn)(uint32_t total_count, uint32_t index, jdm_message_level level, uint32_t line, const char* file, const char* function, const char* message, void* param);
typedef int (*jdm_error_hook_fn)(const char* thread_name, uint32_t stack_trace_count, const char*const* stack_trace, jdm_message_level level, uint32_t line, const char* file, const char* function, const char* message, void* param);

GCC_ONLY(__attribute__((nonnull(1)))) void jdm_process(jdm_error_report_fn function, void* param);
GCC_ONLY(__attribute__((nonnull(1)))) void jdm_peek(jdm_error_report_fn function, void* param);
GCC_ONLY(__attribute__((nonnull(1)))) void jdm_set_hook(jdm_error_hook_fn function, void* param);

#define JDM_ERRNO_MESSAGE strerror(errno)
#define JDM_CRIT(fmt, ...) jdm_push(JDM_ERROR_LEVEL_CRIT, __LINE__, __FILE__, __func__, fmt __VA_OPT__(,) __VA_ARGS__)
#define JDM_ERROR(fmt, ...) jdm_push(JDM_ERROR_LEVEL_ERR, __LINE__, __FILE__, __func__, fmt __VA_OPT__(,) __VA_ARGS__)
#define JDM_WARN(fmt, ...) jdm_push(JDM_ERROR_LEVEL_WARN, __LINE__, __FILE__, __func__, fmt __VA_OPT__(,) __VA_ARGS__)
#define JDM_INFO(fmt, ...) jdm_push(JDM_ERROR_LEVEL_INFO, __LINE__, __FILE__, __func__, fmt __VA_OPT__(,) __VA_ARGS__)
#define JDM_TRACE(fmt, ...) jdm_push(JDM_ERROR_LEVEL_TRACE, __LINE__, __FILE__, __func__, fmt __VA_OPT__(,) __VA_ARGS__)
#define JDM_DEBUG(fmt, ...) jdm_push(JDM_ERROR_LEVEL_DEBUG, __LINE__, __FILE__, __func__, fmt __VA_OPT__(,) __VA_ARGS__)
#define JDM_FATAL(fmt, ...) jdm_report_fatal("Fatal error in %s:%i: "fmt "\n", __FILE__, __LINE__ __VA_OPT__(,) __VA_ARGS__)

#if !defined(NDEBUG) || defined(JDM_STACKTRACE)
#define JDM_ENTER_FUNCTION const uint32_t _jdm_stacktrace_level = jdm_enter_function(__func__)
#define JDM_LEAVE_FUNCTION jdm_leave_function(__func__, _jdm_stacktrace_level)
#else
#define JDM_ENTER_FUNCTION (void)0
#define JDM_LEAVE_FUNCTION (void)0
#endif
#endif //JDM_JDM_H
