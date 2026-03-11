#include <iostream>
#include <string>
#include <csignal>
#include <atomic>

#include "server/mcp_grpc_server.hpp"
#include "utils/logger.hpp"
#include "utils/config_loader.hpp"

// ============================================================
// Graceful shutdown flag
// ============================================================
std::atomic<bool> g_shutdown{false};

void signal_handler(int signal) {
    LOG_INFO("Received signal {}. Initiating graceful shutdown...", signal);
    g_shutdown.store(true);
}

// ============================================================
// Entry Point
// ============================================================
int main(int argc, char* argv[]) {

    // --- Setup signal handlers ---
    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    // --- Load configuration ---
    std::string config_file = (argc > 1) ? argv[1] : "config/server.json";

    auto config = ConfigLoader::load(config_file);
    if (!config) {
        std::cerr << "[ERROR] Failed to load config from: " << config_file << std::endl;
        return EXIT_FAILURE;
    }

    // --- Initialize logger ---
    Logger::init(config->log_level, config->log_file);
    LOG_INFO("==============================================");
    LOG_INFO(" Kafka MCP gRPC Server v1.0.0");
    LOG_INFO(" Kafka broker : {}", config->kafka_brokers);
    LOG_INFO(" gRPC address : {}", config->grpc_listen_address);
    LOG_INFO("==============================================");

    // --- Create and start MCP gRPC server ---
    MCPGrpcServer server(*config);

    if (!server.start()) {
        LOG_ERROR("Failed to start MCPGrpcServer");
        return EXIT_FAILURE;
    }

    LOG_INFO("Server is running. Press Ctrl+C to stop.");

    // --- Wait for shutdown signal ---
    while (!g_shutdown.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // --- Graceful shutdown ---
    LOG_INFO("Shutting down server...");
    server.stop();
    LOG_INFO("Server stopped. Goodbye!");

    return EXIT_SUCCESS;
}
