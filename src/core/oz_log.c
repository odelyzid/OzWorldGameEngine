// Enable POSIX prototypes for functions like localtime_r and fileno
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "../../include/oz/oz_log.h"

#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno

#else
#include <unistd.h>
#endif

static const char *level_to_string(OzLogLevel level) {
  switch (level) {
  case OZ_LOG_TRACE:
    return "TRACE";
  case OZ_LOG_DEBUG:
    return "DEBUG";
  case OZ_LOG_INFO:
    return "INFO";
  case OZ_LOG_WARN:
    return "WARN";
  case OZ_LOG_ERROR:
    return "ERROR";
  case OZ_LOG_FATAL:
    return "FATAL";
  default:
    return "LOG";
  }
}

static const char *level_to_color(OzLogLevel level) {

  switch (level) {
  case OZ_LOG_TRACE:
    return "\x1b[90m"; // bright black
  case OZ_LOG_DEBUG:
    return "\x1b[36m"; // cyan
  case OZ_LOG_INFO:
    return "\x1b[32m"; // green
  case OZ_LOG_WARN:
    return "\x1b[33m"; // yellow
  case OZ_LOG_ERROR:
    return "\x1b[31m"; // red
  case OZ_LOG_FATAL:
    return "\x1b[35m"; // magenta
  default:
    return "";
  }
}

static OzLogSink g_sink = NULL;
static int g_also_stderr = 1;

void oz_log_set_sink(OzLogSink sink, bool also_stderr) {
  g_sink = sink;
  g_also_stderr = also_stderr ? 1 : 0;
}

void oz_log_logv(OzLogLevel level, const char *fmt, va_list args) {
  char timebuf[32];
  time_t now = time(NULL);
  struct tm tm_now;
#if defined(_WIN32)
  localtime_s(&tm_now, &now);
#else
  localtime_r(&now, &tm_now);
#endif
  strftime(timebuf, sizeof(timebuf), "%H:%M:%S", &tm_now);

  // Format into a temporary buffer once
  char msgbuf[1024];
  vsnprintf(msgbuf, sizeof(msgbuf), fmt, args);

  // Build the final formatted line
  char linebuf[1200];
  snprintf(linebuf, sizeof(linebuf), "[%s] %s: %s", timebuf,
           level_to_string(level), msgbuf);

  if (g_sink) {
    g_sink(level, linebuf);
  }

  if (g_also_stderr) {
    int use_color = isatty(fileno(stderr));
    if (use_color)
      fprintf(stderr, "%s", level_to_color(level));
    fprintf(stderr, "%s", linebuf);
    if (use_color)
      fprintf(stderr, "\x1b[0m");
    fputc('\n', stderr);
  }
}

void oz_log_log(OzLogLevel level, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  oz_log_logv(level, fmt, args);
  va_end(args);
}
