#include "utils/logger.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <vector>
#include <memory>

void Logger::init(const std::string& level, const std::string& log_file) {
    std::vector<spdlog::sink_ptr> sinks;

    // Always log to stdout
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

    // Optionally log to rotating file (10MB, 3 files)
    if (!log_file.empty()) {
        sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_file, 10 * 1024 * 1024, 3));
    }

    auto logger = std::make_shared<spdlog::logger>(
        "kafka_mcp", sinks.begin(), sinks.end());

    // Parse level string
    auto lvl = spdlog::level::from_str(level);
    logger->set_level(lvl);
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");

    spdlog::set_default_logger(logger);
    spdlog::flush_on(spdlog::level::warn);

    spdlog::info("Logger initialised — level: {}", level);
}

void Logger::shutdown() {
    spdlog::shutdown();
}
