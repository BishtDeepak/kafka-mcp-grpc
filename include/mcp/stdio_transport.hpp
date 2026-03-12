#pragma once

#include <string>
#include <memory>
#include <atomic>
#include <functional>

#include <nlohmann/json.hpp>

#include "utils/config_loader.hpp"

// ============================================================
// StdioTransport
//
// Implements the Claude Desktop MCP stdio transport.
// Claude Desktop spawns this process and communicates via:
//   stdin  → JSON-RPC 2.0 requests  (one per line)
//   stdout → JSON-RPC 2.0 responses (one per line)
//
// CRITICAL RULES for stdio MCP transport:
//   - stdout is ONLY for JSON-RPC responses — never log here
//   - All logging MUST go to stderr or a file
//   - Each message is a single line of JSON (newline delimited)
//   - Process runs until stdin is closed (Claude Desktop exits)
// ============================================================
class StdioTransport {
public:
    explicit StdioTransport(const ServerConfig& config);
    ~StdioTransport() = default;

    // Start the read loop — blocks until stdin closes
    void run();

    // Signal graceful shutdown
    void stop();

private:
    // --- JSON-RPC message dispatchers ---
    nlohmann::json handle_message(const nlohmann::json& request);

    // MCP protocol method handlers
    nlohmann::json handle_initialize(const nlohmann::json& params, const nlohmann::json& id);
    nlohmann::json handle_list_tools(const nlohmann::json& params, const nlohmann::json& id);
    nlohmann::json handle_call_tool(const nlohmann::json& params, const nlohmann::json& id);
    nlohmann::json handle_ping(const nlohmann::json& id);

    // --- JSON-RPC response builders ---
    nlohmann::json make_result(const nlohmann::json& id, const nlohmann::json& result);
    nlohmann::json make_error(const nlohmann::json& id, int code,
                              const std::string& message);

    // --- Tool schema builders (for ListTools response) ---
    nlohmann::json build_tool_schemas();

    // --- Write a response to stdout (thread-safe) ---
    void write_response(const nlohmann::json& response);

    // --- Members ---
    ServerConfig        config_;
    std::atomic<bool>   running_{false};
};
