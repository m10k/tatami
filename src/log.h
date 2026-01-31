#ifndef LOG_H
#define LOG_H

#include <stdarg.h>

#define LOG_DEBUG    4
#define LOG_INFO     3
#define LOG_WARN     2
#define LOG_ERROR    1
#define LOG_CRITICAL 0
#define LOG_FORCE    -1

void log_set_level(const int log_level);

int log_write(const int level, const char *tag,
              const char *file, const int line,
              const char *format, ...);

#define log_critical(t, f, ...) log_write(LOG_CRITICAL, (t), NULL,     0,        (f), ##__VA_ARGS__)
#define log_error(t, f, ...)    log_write(LOG_ERROR,    (t), NULL,     0,        (f), ##__VA_ARGS__)
#define log_warn(t, f, ...)     log_write(LOG_WARN,     (t), NULL,     0,        (f), ##__VA_ARGS__)
#define log_info(t, f, ...)     log_write(LOG_INFO,     (t), NULL,     0,        (f), ##__VA_ARGS__)
#define log_debug(t, f, ...)    log_write(LOG_DEBUG,    (t), __FILE__, __LINE__, (f), ##__VA_ARGS__)

#endif /* LOG_H */
