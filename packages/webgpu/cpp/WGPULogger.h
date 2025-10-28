//
// Created by Christian Falch on 26/08/2021.
//

#pragma once

#include <cstdio>
#include <jsi/jsi.h>
#include <string>

#if defined(ANDROID) || defined(__ANDROID__)
#include <android/log.h>
#endif

#ifdef __APPLE__
#include <syslog.h>
#endif

namespace rnwgpu {

namespace jsi = facebook::jsi;

class Logger {
public:
  /**
   * Logs message to console
   * @param message Message to be written out
   */
  static void logToConsole(std::string message) {
#ifdef DEBUG
#if defined(ANDROID) || defined(__ANDROID__)
    __android_log_write(ANDROID_LOG_INFO, "WebGPU", message.c_str());
#endif

#ifdef __APPLE__
    syslog(LOG_ERR, "%s\n", message.c_str());
#endif
#else
    // In release mode, do nothing
    (void)message;
#endif
  }

  /**
   * Logs to console
   * @param fmt Format string
   * @param ... Arguments to format string
   */
  static void logToConsole(const char *fmt, ...) {
#ifdef DEBUG
    va_list args;
    va_start(args, fmt);

    static char buffer[512];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
#if defined(ANDROID) || defined(__ANDROID__)
    __android_log_write(ANDROID_LOG_INFO, "WebGPU", buffer);
#endif
#ifdef __APPLE__
    syslog(LOG_ERR, "WebGPU: %s\n", buffer);
#endif
    va_end(args);
#else
    // In release mode, do nothing
    (void)fmt;
#endif
  }

  static void logToJavascriptConsole(jsi::Runtime &runtime,
                                     const std::string &message) {
    auto console = Logger::getJavascriptConsole(runtime).asObject(runtime);
    auto log = console.getPropertyAsFunction(runtime, "log");
    log.call(runtime, jsi::String::createFromUtf8(runtime, message));
  }

  static void warnToJavascriptConsole(jsi::Runtime &runtime,
                                      const std::string &message) {
    auto console = Logger::getJavascriptConsole(runtime).asObject(runtime);
    auto warn = console.getPropertyAsFunction(runtime, "warn");
    warn.call(runtime, jsi::String::createFromUtf8(runtime, message));
  }

  static void errorToJavascriptConsole(jsi::Runtime &runtime,
                                      const std::string &message) {
    auto console = Logger::getJavascriptConsole(runtime).asObject(runtime);
    auto warn = console.getPropertyAsFunction(runtime, "error");
    warn.call(runtime, jsi::String::createFromUtf8(runtime, message));
  }

private:
  static jsi::Value getJavascriptConsole(jsi::Runtime &runtime) {
    auto console = runtime.global().getProperty(runtime, "console");
    if (console.isUndefined() || console.isNull()) {
      throw jsi::JSError(runtime, "Could not find console object.");
      return jsi::Value::undefined();
    }
    return console;
  }
};
} // namespace rnwgpu
