/* logc.h — single-header logging library for C
 *
 * Usage:
 *
 *   In ONE .c file:
 *     #define LOGC_IMPLEMENTATION
 *     #include "logc.h"
 *
 *   In every other file:
 *     #include "logc.h"
 *
 *   Then:
 *     LOG_INFO("server starting on port %d", port);
 *     LOG_ERROR("failed to open '%s': %s", path, strerror(errno));
 *     LOG_FATAL("unrecoverable: %s", reason);   // also calls exit(1)
 *     LOG_BLANK();                              // visual separator (always shown)
 *
 * Features:
 *   - 6 levels: TRACE, DEBUG, INFO, WARN, ERROR, FATAL
 *   - Colored, timestamped output (auto-disabled when stderr is not a TTY)
 *   - Optional file:line:function prefix (logc_set_show_location)
 *   - Runtime min-level filter (logc_set_level)
 *   - Optional log file mirror (logc_set_file)
 *   - Compile-time level cutoff via LOGC_MIN_LEVEL (zero overhead)
 *   - LOG_FATAL aborts with exit(1) automatically
 *   - LOG_ASSERT(cond, msg, ...) — runtime assert that logs and aborts
 *   - LOG_ONCE(level, msg, ...) — logs only on first call
 *   - LOG_BLANK() / LOG_LINE — visual section separators (ALWAYS shown)
 *   - Thread-safe when LOGC_THREAD_SAFE is defined
 */

#ifndef LOGC_H
#define LOGC_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LOGC_TRACE = 0,
    LOGC_DEBUG = 1,
    LOGC_INFO  = 2,
    LOGC_WARN  = 3,
    LOGC_ERROR = 4,
    LOGC_FATAL = 5,
    LOGC_OFF   = 6
} LogLevel;

/* ---- Configuration ---- */

void logc_set_level(LogLevel level);
LogLevel logc_get_level(void);
void logc_set_show_location(bool enable);
void logc_set_show_time(bool enable);
void logc_set_color(int mode);
void logc_set_file(FILE* f);

/* Configure the visual separator printed by LOG_BLANK/LOG_LINE.
 * Default: "$" — single character, dim gray.
 * Pass NULL to reset to default. */
void logc_set_blank_marker(const char* marker);

/* ---- Internal API (use the macros below) ---- */

void logc_print(LogLevel level, const char* file, int line,
                const char* func, const char* fmt, ...)
    __attribute__((format(printf, 5, 6)));

/* Prints a visual separator. ALWAYS prints — does NOT honor level filter,
 * because it is a structural element used to separate phases visually. */
void logc_blank(void);

const char* logc_level_name(LogLevel level);
const char* logc_level_color(LogLevel level);

#ifndef LOGC_MIN_LEVEL
#define LOGC_MIN_LEVEL LOGC_TRACE
#endif

#define LOGC_LOG_(level, fmt, ...)                                          \
    do {                                                                     \
        if ((level) >= LOGC_MIN_LEVEL) {                                     \
            logc_print((level), __FILE__, __LINE__, __func__,                \
                       (fmt), ##__VA_ARGS__);                                \
        }                                                                    \
    } while (0)

#define LOG_TRACE(fmt, ...) LOGC_LOG_(LOGC_TRACE, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) LOGC_LOG_(LOGC_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  LOGC_LOG_(LOGC_INFO,  fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  LOGC_LOG_(LOGC_WARN,  fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOGC_LOG_(LOGC_ERROR, fmt, ##__VA_ARGS__)

/* Visual separator. Both names work — pick the one you like.
 * NO level gating: always prints. */
#define LOG_BLANK    logc_blank()
#define LOG_LINE     logc_blank()

#define LOG_FATAL(fmt, ...)                                                  \
    do {                                                                     \
        logc_print(LOGC_FATAL, __FILE__, __LINE__, __func__,                 \
                   (fmt), ##__VA_ARGS__);                                    \
        exit(1);                                                             \
    } while (0)

#define LOG_ASSERT(cond, fmt, ...)                                           \
    do {                                                                     \
        if (!(cond)) {                                                       \
            logc_print(LOGC_FATAL, __FILE__, __LINE__, __func__,             \
                       "assertion failed: " #cond " — " fmt, ##__VA_ARGS__); \
            exit(1);                                                         \
        }                                                                    \
    } while (0)

#define LOG_ONCE(level, fmt, ...)                                            \
    do {                                                                     \
        static bool _logc_once_##__LINE__ = false;                           \
        if (!_logc_once_##__LINE__) {                                        \
            _logc_once_##__LINE__ = true;                                    \
            LOGC_LOG_((level), fmt, ##__VA_ARGS__);                          \
        }                                                                    \
    } while (0)

/* =====================================================================
 * IMPLEMENTATION
 * ===================================================================== */
#ifdef LOGC_IMPLEMENTATION

#include <time.h>
#include <string.h>
#include <unistd.h>

#ifdef LOGC_THREAD_SAFE
#include <pthread.h>
static pthread_mutex_t logc_mutex_ = PTHREAD_MUTEX_INITIALIZER;
#define LOGC_LOCK()   pthread_mutex_lock(&logc_mutex_)
#define LOGC_UNLOCK() pthread_mutex_unlock(&logc_mutex_)
#else
#define LOGC_LOCK()   ((void)0)
#define LOGC_UNLOCK() ((void)0)
#endif

static const char* logc_colors_[] = {
    "\x1b[1;90m", /* TRACE  - bold gray   */
    "\x1b[1;36m", /* DEBUG  - bold cyan   */
    "\x1b[1;32m", /* INFO   - bold green  */
    "\x1b[1;33m", /* WARN   - bold yellow */
    "\x1b[1;31m", /* ERROR  - bold red    */
    "\x1b[1;35m", /* FATAL  - bold magenta*/
};

static const char* logc_names_[] = {
    "TRACE", "DEBUG", " INFO", " WARN", "ERROR", "FATAL"
};

static LogLevel    logc_min_level_    = LOGC_TRACE;
static bool        logc_show_loc_     = false;
static bool        logc_show_time_    = true;
static int         logc_color_mode_   = -1;
static FILE*       logc_extra_file_   = NULL;
static const char* logc_blank_marker_ = "$";

void logc_set_level(LogLevel level)      { logc_min_level_ = level; }
LogLevel logc_get_level(void)            { return logc_min_level_; }
void logc_set_show_location(bool enable) { logc_show_loc_ = enable; }
void logc_set_show_time(bool enable)     { logc_show_time_ = enable; }
void logc_set_color(int mode)            { logc_color_mode_ = mode; }
void logc_set_file(FILE* f)              { logc_extra_file_ = f; }

void logc_set_blank_marker(const char* marker) {
    logc_blank_marker_ = (marker == NULL) ? "$" : marker;
}

const char* logc_level_name(LogLevel level) {
    if (level < LOGC_TRACE || level > LOGC_FATAL) return "?????";
    return logc_names_[level];
}

const char* logc_level_color(LogLevel level) {
    if (level < LOGC_TRACE || level > LOGC_FATAL) return "\x1b[0m";
    return logc_colors_[level];
}

static bool logc_should_color_(void) {
    if (logc_color_mode_ == 0) return false;
    if (logc_color_mode_ == 1) return true;
    return isatty(fileno(stderr));
}

static const char* logc_basename_(const char* path) {
    const char* slash = strrchr(path, '/');
    if (slash) return slash + 1;
#if defined(_WIN32) || defined(_WIN64)
    const char* bslash = strrchr(path, '\\');
    if (bslash) return bslash + 1;
#endif
    return path;
}

void logc_blank(void) {
    /* NO LEVEL GATING — structural separator, always prints. */
    LOGC_LOCK();

    bool use_color = logc_should_color_();
    /* Dim + gray for a deliberately faint marker. */
    const char* faint = use_color ? "\x1b[2;90m" : "";
    const char* reset = use_color ? "\x1b[0m"   : "";

    fprintf(stderr, "%s%s%s\n", faint, logc_blank_marker_, reset);
    fflush(stderr);

    if (logc_extra_file_) {
        fprintf(logc_extra_file_, "%s\n", logc_blank_marker_);
        fflush(logc_extra_file_);
    }

    LOGC_UNLOCK();
}

void logc_print(LogLevel level, const char* file, int line,
                const char* func, const char* fmt, ...) {
    if (level < logc_min_level_) return;

    LOGC_LOCK();

    bool use_color = logc_should_color_();
    const char* gray  = use_color ? "\x1b[90m" : "";
    const char* dim   = use_color ? "\x1b[2m"  : "";
    const char* reset = use_color ? "\x1b[0m"  : "";
    const char* color = use_color ? logc_level_color(level) : "";

    if (logc_show_time_) {
        time_t now = time(NULL);
        struct tm tm_buf;
#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&tm_buf, &now);
#else
        localtime_r(&now, &tm_buf);
#endif
        fprintf(stderr, "%s[%02d:%02d:%02d]%s ",
                gray, tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec, reset);
    }

    fprintf(stderr, "%s%s%s ", color, logc_level_name(level), reset);

    if (logc_show_loc_ && file) {
        fprintf(stderr, "%s%s:%d in %s()%s ",
                dim, logc_basename_(file), line, func ? func : "?", reset);
    }

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fputc('\n', stderr);
    fflush(stderr);

    if (logc_extra_file_) {
        time_t now = time(NULL);
        struct tm tm_buf;
#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&tm_buf, &now);
#else
        localtime_r(&now, &tm_buf);
#endif
        fprintf(logc_extra_file_, "[%04d-%02d-%02d %02d:%02d:%02d] %s ",
                tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
                tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
                logc_level_name(level));
        if (logc_show_loc_ && file) {
            fprintf(logc_extra_file_, "%s:%d in %s() ",
                    logc_basename_(file), line, func ? func : "?");
        }
        va_list args2;
        va_start(args2, fmt);
        vfprintf(logc_extra_file_, fmt, args2);
        va_end(args2);
        fputc('\n', logc_extra_file_);
        fflush(logc_extra_file_);
    }

    LOGC_UNLOCK();
}

#endif /* LOGC_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif

#endif /* LOGC_H */
