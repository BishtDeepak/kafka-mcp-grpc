
#include "mcp/stdio_transport.hpp"
#include "mcp/mcp_dispatcher.hpp"
#include "utils/logger.hpp"

#include <iostream>
#include <sstream>
#include <mutex>

using json = nlohmann::json;

// ============================================================
// JSON-RPC 2.0 error codes
// ============================================================
static constexpr int RPC_PARSE_ERROR      = -32700;
static constexpr int RPC_INVALID_REQUEST  = -32600;
static constexpr int RPC_METHOD_NOT_FOUND = -32601;
static constexpr int RPC_INVALID_PARAMS   = -32602;
static constexpr int RPC_INTERNAL_ERROR   = -32603;

// stdout mutex — only one thread writes at a time
static std::mutex g_stdout_mutex;

// ============================================================
// Constructor
// ============================================================
StdioTransport::StdioTransport(const ServerConfig& config)
    : config_(config)
{}

// ============================================================
// run()
// Main read loop. Reads one JSON line at a time from stdin.
// Each line is a complete JSON-RPC 2.0 message.
// Blocks until stdin is closed (Claude Desktop disconnects).
// ============================================================
void StdioTransport::run() {
    running_ = true;
    LOG_INFO("StdioTransport: ready, waiting for messages on stdin");

    std::string line;
    while (running_ && std::getline(std::cin, line)) {

        // Skip empty lines
        if (line.empty() || line.find_first_not_of(" \t\r\n") == std::string::npos) {
            continue;
        }

        // Parse JSON
        json request;
        try {
            request = json::parse(line);
        } catch (const json::parse_error& e) {
            LOG_WARN("StdioTransport: JSON parse error: {}", e.what());
            write_response(make_error(nullptr, RPC_PARSE_ERROR,
                "Parse error: " + std::string(e.what())));
            continue;
        }

        // Handle and respond
        try {
            json response = handle_message(request);
            // Notifications have no id — no response needed
            if (!response.is_null()) {
                write_response(response);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("StdioTransport: unhandled exception: {}", e.what());
            auto id = request.contains("id") ? request["id"] : nullptr;
            write_response(make_error(id, RPC_INTERNAL_ERROR, e.what()));
        }
    }

    LOG_INFO("StdioTransport: stdin closed, shutting down");
    running_ = false;
}

void StdioTransport::stop() {
    running_ = false;
}

// ============================================================
// handle_message()
// Routes a JSON-RPC 2.0 request to the right handler.
// ============================================================
json StdioTransport::handle_message(const json& req) {

    // Validate JSON-RPC envelope
    if (!req.contains("jsonrpc") || req["jsonrpc"] != "2.0") {
        auto id = req.contains("id") ? req["id"] : nullptr;
        return make_error(id, RPC_INVALID_REQUEST, "Invalid JSON-RPC version");
    }

    if (!req.contains("method") || !req["method"].is_string()) {
        auto id = req.contains("id") ? req["id"] : nullptr;
        return make_error(id, RPC_INVALID_REQUEST, "Missing or invalid 'method'");
    }

    std::string method = req["method"].get<std::string>();
    json params  = req.value("params", json::object());
    json id      = req.contains("id") ? req["id"] : json(nullptr);

    LOG_DEBUG("StdioTransport: method={}", method);

    // --- Route to handler ---
    if (method == "initialize") {
        return handle_initialize(params, id);
    }
    else if (method == "notifications/initialized") {
        // Claude sends this after initialize — it's a notification, no response
        LOG_INFO("StdioTransport: client initialized");
        return nullptr;
    }
    else if (method == "tools/list") {
        return handle_list_tools(params, id);
    }
    else if (method == "tools/call") {
        return handle_call_tool(params, id);
    }
    else if (method == "ping") {
        return handle_ping(id);
    }
    else {
        LOG_WARN("StdioTransport: unknown method '{}'", method);
        return make_error(id, RPC_METHOD_NOT_FOUND,
            "Method not found: " + method);
    }
}

// ============================================================
// handle_initialize()
// Claude Desktop sends this first — we respond with our
// server capabilities and protocol version.
// ============================================================
json StdioTransport::handle_initialize(const json& params, const json& id) {
    LOG_INFO("StdioTransport: initialize request from client");

    // Log what client sent (useful for debugging)
    if (params.contains("clientInfo")) {
        LOG_INFO("StdioTransport: client={}, version={}",
            params["clientInfo"].value("name", "unknown"),
            params["clientInfo"].value("version", "unknown"));
    }

    json result = {
        {"protocolVersion", "2024-11-05"},
        {"capabilities", {
            {"tools", {
                {"listChanged", false}
            }}
        }},
        {"serverInfo", {
            {"name",    "kafka-mcp-server"},
            {"version", "1.0.0"}
        }}
    };

    return make_result(id, result);
}

// ============================================================
// handle_list_tools()
// Returns all available Kafka tools with their JSON schemas.
// This is what Claude reads to know what it can do.
// ============================================================
json StdioTransport::handle_list_tools(const json& /*params*/, const json& id) {
    LOG_DEBUG("StdioTransport: tools/list");

    json result = {
        {"tools", build_tool_schemas()}
    };

    return make_result(id, result);
}

// ============================================================
// handle_call_tool()
// Claude calls a tool — we dispatch to MCPDispatcher
// which calls the right Kafka API and returns normalized JSON.
// ============================================================
json StdioTransport::handle_call_tool(const json& params, const json& id) {

    // Validate params
    if (!params.contains("name") || !params["name"].is_string()) {
        return make_error(id, RPC_INVALID_PARAMS, "Missing tool 'name'");
    }

    std::string tool_name = params["name"].get<std::string>();
    json tool_args = params.value("arguments", json::object());

    LOG_INFO("StdioTransport: tools/call name={}", tool_name);

    // Dispatch to Kafka layer
    MCPDispatcher dispatcher(config_);
    auto [success, content, error] = dispatcher.dispatch(tool_name, tool_args);

    // Build MCP tool result
    json result;
    if (success) {
        result = {
            {"content", json::array({
                {
                    {"type", "text"},
                    {"text", content}
                }
            })},
            {"isError", false}
        };
    } else {
        result = {
            {"content", json::array({
                {
                    {"type", "text"},
                    {"text", "Error: " + error}
                }
            })},
            {"isError", true}
        };
    }

    return make_result(id, result);
}

// ============================================================
// handle_ping()
// ============================================================
json StdioTransport::handle_ping(const json& id) {
    return make_result(id, json::object());
}

// ============================================================
// build_tool_schemas()
// Returns JSON array of tool definitions — name, description,
// and inputSchema (JSON Schema) for each Kafka tool.
// Claude reads these to know how to call each tool.
// ============================================================
json StdioTransport::build_tool_schemas() {
    return json::array({

        // --- consume_messages ---
        {
            {"name", "consume_messages"},
            {"description", "Read messages from a Kafka topic. Returns up to max_messages messages from the specified topic and partition."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"topic",        {{"type","string"},  {"description","Kafka topic name"}}},
                    {"partition",    {{"type","integer"}, {"description","Partition number. -1 for all partitions"}, {"default", -1}}},
                    {"offset",       {{"type","integer"}, {"description","Start offset. -1 for latest, -2 for earliest"}, {"default", -1}}},
                    {"max_messages", {{"type","integer"}, {"description","Max messages to return (1-100)"}, {"default", 10}}},
                    {"timeout_ms",   {{"type","integer"}, {"description","Timeout in milliseconds"}, {"default", 5000}}}
                }},
                {"required", {"topic"}}
            }}
        },

        // --- produce_message ---
        {
            {"name", "produce_message"},
            {"description", "Publish a message to a Kafka topic."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"topic",     {{"type","string"}, {"description","Target Kafka topic"}}},
                    {"value",     {{"type","string"}, {"description","Message payload (string or JSON string)"}}},
                    {"key",       {{"type","string"}, {"description","Optional message key for partitioning"}}},
                    {"partition", {{"type","integer"}, {"description","Target partition. -1 for auto"}, {"default", -1}}}
                }},
                {"required", {"topic", "value"}}
            }}
        },

        // --- list_topics ---
        {
            {"name", "list_topics"},
            {"description", "List all topics available in the Kafka cluster."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"filter", {{"type","string"}, {"description","Optional prefix filter for topic names"}}}
                }}
            }}
        },

        // --- describe_topic ---
        {
            {"name", "describe_topic"},
            {"description", "Get detailed metadata for a specific Kafka topic including partitions, offsets and replication."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"topic", {{"type","string"}, {"description","Topic name to describe"}}}
                }},
                {"required", {"topic"}}
            }}
        },

        // --- list_consumer_groups ---
        {
            {"name", "list_consumer_groups"},
            {"description", "List all consumer groups registered in the Kafka cluster."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", json::object()}
            }}
        },

        // --- consumer_group_lag ---
        {
            {"name", "consumer_group_lag"},
            {"description", "Get the consumer lag for a specific consumer group. Shows how far behind each partition is from the latest offset."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"group_id", {{"type","string"}, {"description","Consumer group ID"}}},
                    {"topic",    {{"type","string"}, {"description","Optional: filter by topic name"}}}
                }},
                {"required", {"group_id"}}
            }}
        },

        // --- cluster_metadata ---
        {
            {"name", "cluster_metadata"},
            {"description", "Get Kafka cluster metadata including broker list, controller, and cluster ID."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", json::object()}
            }}
        }
    });
}

// ============================================================
// write_response()
// Writes a single JSON-RPC response to stdout.
// Uses mutex to prevent interleaved output.
// NEVER use std::cout anywhere else — only here.
// ============================================================
void StdioTransport::write_response(const json& response) {
    std::lock_guard<std::mutex> lock(g_stdout_mutex);
    std::cout << response.dump() << "\n";
    std::cout.flush();
}

// ============================================================
// make_result() / make_error()
// ============================================================
json StdioTransport::make_result(const json& id, const json& result) {
    return {
        {"jsonrpc", "2.0"},
        {"id",      id},
        {"result",  result}
    };
}

json StdioTransport::make_error(const json& id, int code,
                                const std::string& message) {
    LOG_WARN("StdioTransport: error {} — {}", code, message);
    return {
        {"jsonrpc", "2.0"},
        {"id",      id},
        {"error", {
            {"code",    code},
            {"message", message}
        }}
    };
}