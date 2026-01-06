#pragma once

#ifdef LOG_TO_SYSLOG
#include <syslog.h>
#else
#include <stdio.h> // IWYU pragma: keep
#endif

// define LOG_TO_SYSLOG to use syslog, otherwise fprintf()
#ifdef LOG_TO_SYSLOG
#define DEBUG_LOG(msg, ...) syslog(LOG_DEBUG, "Debug | " msg "\n", ##__VA_ARGS__)
#define ERROR_LOG(msg, ...) syslog(LOG_ERR, "Error | " msg "\n", ##__VA_ARGS__)
#else
#define DEBUG_LOG(msg, ...) fprintf(stdout, "Debug | " msg "\n", ##__VA_ARGS__)
#define ERROR_LOG(msg, ...) fprintf(stderr, "Error | " msg "\n", ##__VA_ARGS__)
#endif
