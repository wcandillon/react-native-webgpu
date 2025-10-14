#include <cstdio>
#include <sstream>
#include <string>
#include <utility>

#ifdef __ANDROID__
#include <android/log.h>
#elif defined(__APPLE__)
#include <os/log.h>
#endif

namespace dawn {

enum class LogSeverity {
    Debug,
    Info,
    Warning,
    Error,
};

// Forward declare to match Dawn's exact interface
class LogMessage {
public:
    explicit LogMessage(LogSeverity severity);
    ~LogMessage();
    LogMessage(LogMessage&& other);
    LogMessage& operator=(LogMessage&& other);
    template <typename T>
    LogMessage& operator<<(T&& value) {
        mStream << value;
        return *this;
    }

private:
    LogMessage(const LogMessage& other) = delete;
    LogMessage& operator=(const LogMessage& other) = delete;
    LogSeverity mSeverity;
    std::ostringstream mStream;
};

// Implementation of LogMessage methods
LogMessage::LogMessage(LogSeverity severity) : mSeverity(severity) {}

LogMessage::LogMessage(LogMessage&& other)
    : mSeverity(other.mSeverity), mStream(std::move(other.mStream)) {}

LogMessage& LogMessage::operator=(LogMessage&& other) {
    if (this != &other) {
        mSeverity = other.mSeverity;
        mStream = std::move(other.mStream);
    }
    return *this;
}

LogMessage::~LogMessage() {
    std::string fullMessage = mStream.str();
    if (fullMessage.empty()) {
        return;
    }
    const char* severityName;
    switch (mSeverity) {
        case LogSeverity::Debug: severityName = "Debug"; break;
        case LogSeverity::Info: severityName = "Info"; break;
        case LogSeverity::Warning: severityName = "Warning"; break;
        case LogSeverity::Error: severityName = "Error"; break;
        default: severityName = "Unknown"; break;
    }

#ifdef __ANDROID__
    int androidPriority;
    switch (mSeverity) {
        case LogSeverity::Debug: androidPriority = ANDROID_LOG_DEBUG; break;
        case LogSeverity::Info: androidPriority = ANDROID_LOG_INFO; break;
        case LogSeverity::Warning: androidPriority = ANDROID_LOG_WARN; break;
        case LogSeverity::Error: androidPriority = ANDROID_LOG_ERROR; break;
        default: androidPriority = ANDROID_LOG_ERROR; break;
    }
    __android_log_print(androidPriority, "ReactNativeWebGPU", "%s: %s", severityName, fullMessage.c_str());
#elif defined(__APPLE__)
    os_log_type_t logType;
    switch (mSeverity) {
        case LogSeverity::Debug: logType = OS_LOG_TYPE_DEBUG; break;
        case LogSeverity::Info: logType = OS_LOG_TYPE_INFO; break;
        case LogSeverity::Warning: logType = OS_LOG_TYPE_DEFAULT; break;
        case LogSeverity::Error: logType = OS_LOG_TYPE_ERROR; break;
        default: logType = OS_LOG_TYPE_ERROR; break;
    }
    os_log_with_type(OS_LOG_DEFAULT, logType, "[ReactNativeWebGPU] %s: %s", severityName, fullMessage.c_str());
#else
    FILE* outputStream = (mSeverity == LogSeverity::Warning || mSeverity == LogSeverity::Error) ? stderr : stdout;
    fprintf(outputStream, "[ReactNativeWebGPU] %s: %s\n", severityName, fullMessage.c_str());
    fflush(outputStream);
#endif
}

// Factory functions
LogMessage DebugLog() {
    return LogMessage(LogSeverity::Debug);
}

LogMessage InfoLog() {
    return LogMessage(LogSeverity::Info);
}

LogMessage WarningLog() {
    return LogMessage(LogSeverity::Warning);
}

LogMessage ErrorLog() {
    return LogMessage(LogSeverity::Error);
}

LogMessage DebugLog(const char* file, const char* function, int line) {
    LogMessage message = DebugLog();
    message << file << ":" << line << "(" << function << ")";
    return message;
}

} // namespace dawn