#include "utils/logger.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <vector>

void Logger::init(const std::string& level,
                  const std::string& log_file,
                  bool use_stderr) {

    std::vector<spdlog::sink_ptr> sinks;

    // -------------------------------------------------------
    // Console sink
    // CRITICAL: stdio mode MUST use stderr — stdout is JSON-RPC
    // grpc mode can use stdout for human-readable colored output
    // -------------------------------------------------------
    if (use_stderr) {
        // stderr — safe for stdio MCP mode
        auto sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
        sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
        sinks.push_back(sink);
    } else {
        // stdout — only for grpc mode
        auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
        sinks.push_back(sink);
    }

    // Optional rotating file sink
    if (!log_file.empty()) {
        try {
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                log_file,
                10 * 1024 * 1024,  // 10MB per file
                3                   // keep 3 rotated files
            );
            file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");
            sinks.push_back(file_sink);
        } catch (const spdlog::spdlog_ex& e) {
            // Don't crash — just continue without file logging
        }
    }

    auto logger = std::make_shared<spdlog::logger>(
        "kafka_mcp", sinks.begin(), sinks.end());

    // Set log level
    spdlog::level::level_enum log_level = spdlog::level::info;
    if      (level == "trace")    log_level = spdlog::level::trace;
    else if (level == "debug")    log_level = spdlog::level::debug;
    else if (level == "info")     log_level = spdlog::level::info;
    else if (level == "warn")     log_level = spdlog::level::warn;
    else if (level == "error")    log_level = spdlog::level::err;
    else if (level == "critical") log_level = spdlog::level::critical;

    logger->set_level(log_level);
    logger->flush_on(spdlog::level::warn);
    spdlog::set_default_logger(logger);

    spdlog::info("Logger init — level:{} sink:{} file:{}",
        level,
        use_stderr ? "stderr" : "stdout",
        log_file.empty() ? "none" : log_file);
}

void Logger::shutdown() {
    spdlog::info("Logger shutting down.");
    spdlog::shutdown();
}