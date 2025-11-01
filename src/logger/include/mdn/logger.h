
#ifndef LOGGER_H
#define LOGGER_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdbool.h>
#include <stdio.h>

#include "mdn/status.h"

typedef enum mdn_Logger_loggingLevel_t_ {
    MDN_LOGGER_LOGGING_LEVEL_DEBUG,     // Detailed information for debugging
    MDN_LOGGER_LOGGING_LEVEL_INFO,      // General informational messages
    MDN_LOGGER_LOGGING_LEVEL_WARNING,   // Warnings about potential issues
    MDN_LOGGER_LOGGING_LEVEL_ERROR,     // Errors that need attention
    MDN_LOGGER_LOGGING_LEVEL_CRITICAL,  // Critical issues that cause system failure
    MDN_LOGGER_LOGGING_LEVEL_COUNT      // Total number of logging levels (upper bound)
} mdn_Logger_loggingLevel_t;

typedef enum mdn_Logger_loggingFormat_t_ {
    MDN_LOGGER_LOGGING_FORMAT_SCREEN,
    MDN_LOGGER_LOGGING_FORMAT_FILE,
    MDN_LOGGER_LOGGING_FORMAT_COUNT,
} mdn_Logger_loggingFormat_t;

typedef struct mdn_Logger_StreamConfig_t_ {
    FILE                      *stream;
    mdn_Logger_loggingLevel_t  loggingLevel;
    mdn_Logger_loggingFormat_t loggingFormat;
} mdn_Logger_StreamConfig_t;

#if (!defined MDN_LOGGER_SET_LEVEL_DEBUG) && (!defined MDN_LOGGER_SET_LEVEL_INFO) && (!defined MDN_LOGGER_SET_LEVEL_WARNING) && (!defined MDN_LOGGER_SET_LEVEL_ERROR) && (!defined MDN_LOGGER_SET_LEVEL_CRITICAL) && (!defined MDN_LOGGER_SET_LEVEL_NONE)
# error Requested minimal logging level must be defined
#endif

#define MDN_LOGGER_FUNC_NAME                 __func__
#define MDN_LOGGER_LOG_COMMON(logLevel, ...) mdn_Logger_log(logLevel, __FILE__, __LINE__, MDN_LOGGER_FUNC_NAME, __VA_ARGS__)

#if (defined MDN_LOGGER_SET_LEVEL_DEBUG)
# define MDN_LOGGER_LOG_DEBUG(...) MDN_LOGGER_LOG_COMMON(MDN_LOGGER_LOGGING_LEVEL_DEBUG, __VA_ARGS__)
#else
# define MDN_LOGGER_LOG_DEBUG(...)
#endif

#if (defined MDN_LOGGER_SET_LEVEL_DEBUG) || (defined MDN_LOGGER_SET_LEVEL_INFO)
# define MDN_LOGGER_LOG_INFO(...) MDN_LOGGER_LOG_COMMON(MDN_LOGGER_LOGGING_LEVEL_INFO, __VA_ARGS__)
#else
# define MDN_LOGGER_LOG_INFO(...)
#endif

#if (defined MDN_LOGGER_SET_LEVEL_DEBUG) || (defined MDN_LOGGER_SET_LEVEL_INFO) || (defined MDN_LOGGER_SET_LEVEL_WARNING)
# define MDN_LOGGER_LOG_WARNING(...) MDN_LOGGER_LOG_COMMON(MDN_LOGGER_LOGGING_LEVEL_WARNING, __VA_ARGS__)
#else
# define MDN_LOGGER_LOG_WARNING(...)
#endif

#if (defined MDN_LOGGER_SET_LEVEL_DEBUG) || (defined MDN_LOGGER_SET_LEVEL_INFO) || (defined MDN_LOGGER_SET_LEVEL_WARNING) || (defined MDN_LOGGER_SET_LEVEL_ERROR)
# define MDN_LOGGER_LOG_ERROR(...) MDN_LOGGER_LOG_COMMON(MDN_LOGGER_LOGGING_LEVEL_ERROR, __VA_ARGS__)
#else
# define MDN_LOGGER_LOG_ERROR(...)
#endif

#if (defined MDN_LOGGER_SET_LEVEL_DEBUG) || (defined MDN_LOGGER_SET_LEVEL_INFO) || (defined MDN_LOGGER_SET_LEVEL_WARNING) || (defined MDN_LOGGER_SET_LEVEL_ERROR) || (defined MDN_LOGGER_SET_LEVEL_CRITICAL)
# define MDN_LOGGER_LOG_CRITICAL(...) MDN_LOGGER_LOG_COMMON(MDN_LOGGER_LOGGING_LEVEL_CRITICAL, __VA_ARGS__)
#else
# define MDN_LOGGER_LOG_CRITICAL(...)
#endif

mdn_Status_t mdn_Logger_init(void);

mdn_Status_t mdn_Logger_deinit(void);

mdn_Status_t mdn_Logger_addOutputStream(mdn_Logger_StreamConfig_t streamConfig);

void mdn_Logger_log(mdn_Logger_loggingLevel_t loggingLevel, const char *file, int line, const char *func, const char *format, ...);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // LOGGER_H
