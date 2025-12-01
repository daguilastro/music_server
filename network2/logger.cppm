module; 

#include <cstdint>
#include <type_traits>
#include <format>
#include <iostream>
#include <mutex>       
#include <chrono>
#include <print>

export module logger;


using LogConfig = std::uint8_t;

constexpr LogConfig LEVEL_MASK = 0x07;

export enum class LoggerLevel : std::uint8_t{
    Debug = 0x00,
    Info = 0x01,
    Warning = 0x02,
    Error = 0x03,
    Off = 0x04,
};

export enum class LoggerFlag : std::uint8_t{
    Stdout = 0x08,
    Stderr = 0x10,
    Timestamp = 0x20,
};

template <typename Type>    
requires std::is_enum_v<Type>
constexpr LogConfig toRawLevel(Type value) noexcept {
    return static_cast<LogConfig>(value);
}

LogConfig globalConfig = toRawLevel(LoggerLevel::Debug) | toRawLevel(LoggerFlag::Stdout);


std::mutex logMutex;

std::uint8_t getLevel(LogConfig configuration) noexcept{
    return configuration & LEVEL_MASK;
}

bool hasFlag(LogConfig configuration, LoggerFlag flag) noexcept{
    return configuration & toRawLevel(flag);
}

constexpr std::string_view levelPrefix(LoggerLevel level) noexcept {
    switch (level) {
        case LoggerLevel::Debug:   return "[DEBUG]";
        case LoggerLevel::Info:    return "[INFO]";
        case LoggerLevel::Warning: return "[WARN]";
        case LoggerLevel::Error:   return "[ERROR]";
        default:                   return "";
    }
}


// ============================================================================
// API pública (exportada)
// ============================================================================

export template <typename... Flags>
requires (std::is_same_v<Flags, LoggerFlag> && ...) && (sizeof...(Flags) > 0)
void configure(LoggerLevel level, Flags&&... flags) noexcept {
    globalConfig = toRawLevel(level);
    globalConfig |= (toRawLevel(flags) | ...);
}

export template <typename... Args>
void debug(std::format_string<Args...> fmt, Args&&... args) noexcept {
    auto message = std::format(fmt, std::forward<Args>(args)...);
    logOutput(LoggerLevel::Debug, message);
}

export template <typename... Args>
void info(std::format_string<Args...> fmt, Args&&... args) noexcept {
    auto message = std::format(fmt, std::forward<Args>(args)...);
    logOutput(LoggerLevel::Info, message);
}

export template <typename... Args>
void warn(std::format_string<Args...> fmt, Args&&... args) noexcept {
    auto message = std::format(fmt, std::forward<Args>(args)...);
    logOutput(LoggerLevel::Warning, message);
}

export template <typename... Args>
void error(std::format_string<Args...> fmt, Args&&... args) noexcept {
    auto message = std::format(fmt, std::forward<Args>(args)...);
    logOutput(LoggerLevel::Error, message);
}

// ============================================================================
// Función principal de salida de logs
// ============================================================================

void logOutput(LoggerLevel level, std::string_view message) noexcept {
    if (getLevel(globalConfig) > toRawLevel(level)) {
        return;
    }

    std::lock_guard<std::mutex> lock(logMutex);

    FILE* output = hasFlag(globalConfig, LoggerFlag::Stderr) ? stderr : stdout;

    if (hasFlag(globalConfig, LoggerFlag::Timestamp)) {
        auto now = std::chrono::system_clock::now();
        auto local_time = std::chrono::zoned_time(std::chrono::current_zone(), now);
        
        std::print(output, "[{:%H:%M:%S}] ", local_time);
    }

    std::print(output, "{} {}\n", levelPrefix(level), message);
}