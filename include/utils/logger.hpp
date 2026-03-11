#pragma once

#include <string>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

// ============================================================
// Logger — thin wrapper around spdlog
// ============================================================
class Logger {
public:
    static void init(const std::string& level = "info",
                     const std::string& log_file = "");
    static void shutdown();
};

// ============================================================
// Convenience macros
// ============================================================
#define LOG_TRACE(...)  spdlog::trace(__VA_ARGS__)
#define LOG_DEBUG(...)  spdlog::debug(__VA_ARGS__)
#define LOG_INFO(...)   spdlog::info(__VA_ARGS__)
#define LOG_WARN(...)   spdlog::warn(__VA_ARGS__)
#define LOG_ERROR(...)  spdlog::error(__VA_ARGS__)
#define LOG_CRITICAL(...) spdlog::critical(__VA_ARGS__)
