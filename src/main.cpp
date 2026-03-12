#include <iostream>
#include <string>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <filesystem>

#include "server/mcp_grpc_server.hpp"
#include "mcp/stdio_transport.hpp"
#include "utils/logger.hpp"
#include "utils/config_loader.hpp"

namespace fs = std::filesystem;

std::atomic<bool> g_shutdown{false};

void signal_handler(int signal) {
    LOG_INFO("Received signal {}. Initiating graceful shutdown...", signal);
    g_shutdown.store(true);
}

// ============================================================
// resolve_config_path()
// Tries CWD first, then binary-relative path.
// Fixes the "ran from build/ and can't find config/" problem.
// ============================================================
std::string resolve_config_path(const char* argv0,
                                const std::string& config_file) {
    if (fs::path(config_file).is_absolute()) return config_file;
    if (fs::exists(config_file))             return config_file;

    // Try relative to binary (e.g. binary=build/, config=../config/)
    fs::path binary_dir = fs::path(argv0).parent_path();
    fs::path candidate  = binary_dir / ".." / config_file;
    std::error_code ec;
    fs::path canonical = fs::canonical(candidate, ec);
    if (!ec && fs::exists(canonical)) return canonical.string();

    return config_file; // return original, let ConfigLoader error clearly
}

void print_usage(const char* prog) {
    std::cerr << "\nUsage:\n"
              << "  " << prog << " [mode] [config_file]\n\n"
              << "Modes:  stdio (default) | grpc\n\n"
              << "Examples:\n"
              << "  " << prog << " stdio\n"
              << "  " << prog << " grpc\n"
              << "  " << prog << " stdio /abs/path/server.json\n\n";
}

int main(int argc, char* argv[]) {

    std::string mode        = "stdio";
    std::string config_file = "config/server.json";

    if (argc == 2) {
        std::string arg = argv[1];
        if (arg == "stdio" || arg == "grpc") mode = arg;
        else config_file = arg;  // treat as config path
    }
    else if (argc >= 3) {
        mode        = argv[1];
        config_file = argv[2];
    }

    if (mode != "stdio" && mode != "grpc") {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    // Resolve config path relative to binary if not found in CWD
    config_file = resolve_config_path(argv[0], config_file);
    std::cerr << "[INFO] Config: " << config_file
              << "  Mode: " << mode << std::endl;

    auto config = ConfigLoader::load(config_file);
    if (!config) {
        std::cerr << "[ERROR] Failed to load config: " << config_file << "\n"
                  << "[HINT]  Run: ./build/kafka_mcp_server stdio\n"
                  << "[HINT]  Or:  ./build/kafka_mcp_server stdio "
                  << "/full/path/to/config/server.json\n";
        return EXIT_FAILURE;
    }

    // CRITICAL: In stdio mode stdout = JSON-RPC only. Logger → stderr/file.
    Logger::init(config->log_level, config->log_file, true);

    if (mode == "stdio") {
        LOG_INFO("=== Kafka MCP Server v1.0.0 [STDIO] ===");
        LOG_INFO("Kafka: {}  MaxMsgs: {}",
                 config->kafka_brokers, config->max_messages_per_request);

        StdioTransport transport(*config);
        transport.run();
        LOG_INFO("STDIO transport exited");

    } else {
        std::signal(SIGINT,  signal_handler);
        std::signal(SIGTERM, signal_handler);

        LOG_INFO("=== Kafka MCP Server v1.0.0 [gRPC] ===");
        LOG_INFO("Kafka: {}  gRPC: {}",
                 config->kafka_brokers, config->grpc_listen_address);

        MCPGrpcServer server(*config);
        if (!server.start()) { LOG_ERROR("gRPC start failed"); return EXIT_FAILURE; }

        LOG_INFO("Running. Ctrl+C to stop.");
        while (!g_shutdown.load())
            std::this_thread::sleep_for(std::chrono::milliseconds(200));

        server.stop();
        LOG_INFO("Stopped.");
    }

    Logger::shutdown();
    return EXIT_SUCCESS;
}
