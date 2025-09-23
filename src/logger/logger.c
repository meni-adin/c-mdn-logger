#define LOGGING_DEBUG_AND_ABOVE
#include "logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#if (defined __APPLE__) || (defined __linux__)
# include <sys/time.h>
#elif defined _WIN32
# include <windows.h>
#endif  // OS

#include "mdn/mock_wrapper.h"

#ifdef MDN_LOGGER_SAFE_MODE
# define IS_VALID_LOGGING_LEVEL(loggingLevel)   ((0 <= (loggingLevel)) && ((loggingLevel) < MDN_LOGGER_LOGGING_LEVEL_COUNT))
# define IS_VALID_LOGGING_FORMAT(loggingFormat) ((0 <= (loggingFormat)) && ((loggingFormat) < MDN_LOGGER_LOGGING_FORMAT_COUNT))
#endif  // MDN_LOGGER_SAFE_MODE

typedef struct Logger_InternalState_t_ {
    mdn_Logger_StreamConfig_t *streamsArr;
    size_t                     streamsArrLen;
} Logger_InternalState_t;

static Logger_InternalState_t *g_Logger_internalState;

typedef struct mdn_Logger_logToStreamArguments_t_ {
    size_t                    streamIndex;
    mdn_Logger_loggingLevel_t loggingLevel;
    const char               *file;
    int                       line;
    const char               *funcName;
    const char               *format;
    va_list                   args;
} mdn_Logger_logToStreamArguments_t;

static const char *g_mdn_Logger_logLevelToStrMap[] = {
    [MDN_LOGGER_LOGGING_LEVEL_DEBUG]    = "DEBUG",
    [MDN_LOGGER_LOGGING_LEVEL_INFO]     = "INFO",
    [MDN_LOGGER_LOGGING_LEVEL_WARNING]  = "WARNING",
    [MDN_LOGGER_LOGGING_LEVEL_ERROR]    = "ERROR",
    [MDN_LOGGER_LOGGING_LEVEL_CRITICAL] = "CRITICAL",
};
#define ARRAY_LEN(array) (sizeof(array) / sizeof(*(array)))
_Static_assert(ARRAY_LEN(g_mdn_Logger_logLevelToStrMap) == MDN_LOGGER_LOGGING_LEVEL_COUNT,
               "Error: seems like a description string for a log level is missing");

#define ANSI_ESC                      "\033"
#define ANSI_PARAM_BEGIN              "["
#define ANSI_PARAM_END                "m"
#define COLOR_PREFIX                  ANSI_ESC ANSI_PARAM_BEGIN
#define COLOR_SUFFIX                  ANSI_PARAM_END
#define COLOR(color)                  COLOR_PREFIX color COLOR_SUFFIX
#define LOGGER_TERMINAL_COLOR_GRAY    COLOR("90")
#define LOGGER_TERMINAL_COLOR_RESET   COLOR("0")
#define LOGGER_TERMINAL_COLOR_YELLOW  COLOR("33")
#define LOGGER_TERMINAL_COLOR_RED     COLOR("31")
#define LOGGER_TERMINAL_COLOR_MAGENTA COLOR("35")

typedef enum mdn_Logger_loggingColor_t_ {
    LOGGING_COLOR_GRAY,
    LOGGING_COLOR_RESET,
    LOGGING_COLOR_YELLOW,
    LOGGING_COLOR_RED,
    LOGGING_COLOR_MAGENTA,
    LOGGING_COLOR_COUNT,
} mdn_Logger_loggingColor_t;

static mdn_Logger_loggingColor_t g_mdn_Logger_loggingLevelToColorMap[] = {
    [MDN_LOGGER_LOGGING_LEVEL_DEBUG]    = LOGGING_COLOR_GRAY,
    [MDN_LOGGER_LOGGING_LEVEL_INFO]     = LOGGING_COLOR_RESET,
    [MDN_LOGGER_LOGGING_LEVEL_WARNING]  = LOGGING_COLOR_YELLOW,
    [MDN_LOGGER_LOGGING_LEVEL_ERROR]    = LOGGING_COLOR_RED,
    [MDN_LOGGER_LOGGING_LEVEL_CRITICAL] = LOGGING_COLOR_MAGENTA,
};

const char *g_Logger_colorToTerminalColorMap[] = {
    [LOGGING_COLOR_GRAY]    = LOGGER_TERMINAL_COLOR_GRAY,
    [LOGGING_COLOR_RESET]   = LOGGER_TERMINAL_COLOR_RESET,
    [LOGGING_COLOR_YELLOW]  = LOGGER_TERMINAL_COLOR_YELLOW,
    [LOGGING_COLOR_RED]     = LOGGER_TERMINAL_COLOR_RED,
    [LOGGING_COLOR_MAGENTA] = LOGGER_TERMINAL_COLOR_MAGENTA,
};

mdn_Status_t mdn_Logger_init(void) {
#ifdef MDN_LOGGER_SAFE_MODE
    if (g_Logger_internalState != NULL) {
        return MDN_STATUS_ERROR_LIBRARY_ALREADY_INITIALIZED;
    }
#endif  // MDN_LOGGER_SAFE_MODE

    g_Logger_internalState = MDN_MW_malloc(sizeof(*g_Logger_internalState));
    if (g_Logger_internalState == NULL) {
        return MDN_STATUS_ERROR_MEM_ALLOC;
    }

    *g_Logger_internalState = (Logger_InternalState_t){
        .streamsArr    = NULL,
        .streamsArrLen = 0,
    };

    return MDN_STATUS_SUCCESS;
}

mdn_Status_t mdn_Logger_deinit(void) {
#ifdef MDN_LOGGER_SAFE_MODE
    if (g_Logger_internalState == NULL) {
        return MDN_STATUS_ERROR_LIBRARY_NOT_INITIALIZED;
    }
#endif  // MDN_LOGGER_SAFE_MODE

    free(g_Logger_internalState->streamsArr);
    free(g_Logger_internalState);
    g_Logger_internalState = NULL;

    return MDN_STATUS_SUCCESS;
}

mdn_Status_t mdn_Logger_addOutputStream(mdn_Logger_StreamConfig_t streamConfig) {
    mdn_Logger_StreamConfig_t *streamsArrTemp;
#ifdef MDN_LOGGER_SAFE_MODE
    if (g_Logger_internalState == NULL) {
        return MDN_STATUS_ERROR_LIBRARY_NOT_INITIALIZED;
    }
    if (streamConfig.stream == NULL) {
        return MDN_STATUS_ERROR_BAD_ARGUMENT;
    }
    if (!IS_VALID_LOGGING_LEVEL(streamConfig.loggingLevel)) {
        return MDN_STATUS_ERROR_BAD_ARGUMENT;
    }
    if (!IS_VALID_LOGGING_FORMAT(streamConfig.loggingFormat)) {
        return MDN_STATUS_ERROR_BAD_ARGUMENT;
    }
#endif  // MDN_LOGGER_SAFE_MODE

    streamsArrTemp                     = g_Logger_internalState->streamsArr;
    g_Logger_internalState->streamsArr = MDN_MW_realloc(g_Logger_internalState->streamsArr, (g_Logger_internalState->streamsArrLen + 1) * sizeof(*(g_Logger_internalState->streamsArr)));
    if (g_Logger_internalState->streamsArr == NULL) {
        g_Logger_internalState->streamsArr = streamsArrTemp;
        return MDN_STATUS_ERROR_MEM_ALLOC;
    }
    (g_Logger_internalState->streamsArr)[g_Logger_internalState->streamsArrLen] = streamConfig;
    ++(g_Logger_internalState->streamsArrLen);

    return MDN_STATUS_SUCCESS;
}

static void mdn_Logger_setColor(FILE *stream, mdn_Logger_loggingColor_t color) {
    fprintf(stream, "%s", g_Logger_colorToTerminalColorMap[color]);
}

static void mdn_Logger_printTimestamp(FILE *stream, bool includeDate) {
#if (defined __APPLE__) || (defined __linux__)
    struct timeval tv;
    struct tm     *tm_info;
    char           timestampBuf[10 + 1 + 8 + 1];  // 10(date: YYYY-MM-DD) + 1(space) + 8(time: HH:MM:SS) + 1(null-terminator)
    const char    *timestampStart  = includeDate ? timestampBuf : timestampBuf + 11;
    int            usecToMsecDenom = 1000;

    if (gettimeofday(&tv, NULL) != 0) {
        tv.tv_sec  = 0;
        tv.tv_usec = 0;
    }
    tm_info = localtime(&tv.tv_sec);
    strftime(timestampBuf, sizeof(timestampBuf), "%Y-%m-%d %H:%M:%S", tm_info);

    // Safe cast: microseconds/1000 gives milliseconds (0-999), fits in int
    (void)fprintf(stream, "%s.%03d ", timestampStart, (int)(tv.tv_usec / usecToMsecDenom));
#elif defined _WIN32
    SYSTEMTIME st;

    GetLocalTime(&st);

    if (includeDate) {
        fprintf(stream, "%04d-%02d-%02d ", st.wYear, st.wMonth, st.wDay);
    }
    fprintf(stream, "%02d:%02d:%02d.%03d ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
#endif  // OS
}

static void mdn_Logger_printLoggingLevel(FILE *stream, mdn_Logger_loggingLevel_t loggingLevel) {
    fprintf(stream, "%-8s ", g_mdn_Logger_logLevelToStrMap[loggingLevel]);
}

static void mdn_Logger_printFuncName(FILE *stream, const char *funcName) {
    fprintf(stream, "%-20s ", funcName);
}

static void mdn_Logger_printSeparator(FILE *stream) {
    fprintf(stream, "| ");
}

static void mdn_Logger_logToScreen(mdn_Logger_logToStreamArguments_t *logToStreamArguments) {
    FILE *stream = g_Logger_internalState->streamsArr[logToStreamArguments->streamIndex].stream;

    mdn_Logger_setColor(stream, g_mdn_Logger_loggingLevelToColorMap[logToStreamArguments->loggingLevel]);
    mdn_Logger_printTimestamp(stream, false);
    mdn_Logger_printFuncName(stream, logToStreamArguments->funcName);
    mdn_Logger_printSeparator(stream);
    (void)vfprintf(stream, logToStreamArguments->format, logToStreamArguments->args);  // NOLINT (allow non-literal string as format)
    mdn_Logger_setColor(stream, LOGGING_COLOR_RESET);
    (void)fprintf(stream, "\n");
}

static void mdn_Logger_logToFile(mdn_Logger_logToStreamArguments_t *logToStreamArguments) {
    FILE *stream = g_Logger_internalState->streamsArr[logToStreamArguments->streamIndex].stream;

    mdn_Logger_printTimestamp(stream, true);
    mdn_Logger_printLoggingLevel(stream, logToStreamArguments->loggingLevel);
    mdn_Logger_printFuncName(stream, logToStreamArguments->funcName);
    mdn_Logger_printSeparator(stream);
    (void)vfprintf(stream, logToStreamArguments->format, logToStreamArguments->args);  // NOLINT (allow non-literal string as format)
    (void)fprintf(stream, "\n");
}

typedef void (*mdn_Logger_logPrintFunc)(mdn_Logger_logToStreamArguments_t *);
static mdn_Logger_logPrintFunc g_mdn_Logger_logFormatToFuncMap[] = {
    [MDN_LOGGER_LOGGING_FORMAT_SCREEN] = mdn_Logger_logToScreen,
    [MDN_LOGGER_LOGGING_FORMAT_FILE]   = mdn_Logger_logToFile,
};

void mdn_Logger_log(mdn_Logger_loggingLevel_t loggingLevel, const char *file, int line, const char *funcName, const char *format, ...) {
    mdn_Logger_logToStreamArguments_t logToStreamArguments = (mdn_Logger_logToStreamArguments_t){
        .loggingLevel = loggingLevel,
        .file         = file,
        .line         = line,
        .funcName     = funcName,
        .format       = format,
    };
    va_list                 args;
    mdn_Logger_logPrintFunc logPrintFunc;

    for (size_t idx = 0; idx < g_Logger_internalState->streamsArrLen; ++idx) {
        if (loggingLevel < g_Logger_internalState->streamsArr[idx].loggingLevel) {
            continue;
        }
        va_start(args, format);
        va_copy(logToStreamArguments.args, args);
        logToStreamArguments.streamIndex = idx;
        logPrintFunc                     = g_mdn_Logger_logFormatToFuncMap[g_Logger_internalState->streamsArr[idx].loggingFormat];
        logPrintFunc(&logToStreamArguments);
        va_end(logToStreamArguments.args);
        va_end(args);
    }
}
