#ifndef OZ_LOG_H
#define OZ_LOG_H

#include <stdarg.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum OzLogLevel {
    OZ_LOG_TRACE = 0,
    OZ_LOG_DEBUG = 1,
    OZ_LOG_INFO  = 2,
    OZ_LOG_WARN  = 3,
    OZ_LOG_ERROR = 4,
    OZ_LOG_FATAL = 5
} OzLogLevel;

void oz_log_log(OzLogLevel level, const char* fmt, ...);
void oz_log_logv(OzLogLevel level, const char* fmt, va_list args);

// Optional sink to receive formatted log lines in addition to stderr
typedef void (*OzLogSink)(OzLogLevel level, const char* formatted_line);
void oz_log_set_sink(OzLogSink sink, bool also_stderr);

#define OZ_TRACE(fmt, ...) oz_log_log(OZ_LOG_TRACE, (fmt), ##__VA_ARGS__)
#define OZ_DEBUG(fmt, ...) oz_log_log(OZ_LOG_DEBUG, (fmt), ##__VA_ARGS__)
#define OZ_INFO(fmt, ...)  oz_log_log(OZ_LOG_INFO,  (fmt), ##__VA_ARGS__)
#define OZ_WARN(fmt, ...)  oz_log_log(OZ_LOG_WARN,  (fmt), ##__VA_ARGS__)
#define OZ_ERROR(fmt, ...) oz_log_log(OZ_LOG_ERROR, (fmt), ##__VA_ARGS__)
#define OZ_FATAL(fmt, ...) oz_log_log(OZ_LOG_FATAL, (fmt), ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // OZ_LOG_H
