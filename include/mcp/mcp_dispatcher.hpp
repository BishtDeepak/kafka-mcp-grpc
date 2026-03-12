#pragma once

#include <string>
#include <tuple>
#include <memory>

#include <nlohmann/json.hpp>

#include "kafka/kafka_consumer.hpp"
#include "kafka/kafka_producer.hpp"
#include "kafka/kafka_admin.hpp"
#include "utils/config_loader.hpp"

// ============================================================
// DispatchResult
// Simple tuple: success, content (JSON string), error message
// ============================================================
using DispatchResult = std::tuple<bool, std::string, std::string>;

// ============================================================
// MCPDispatcher
//
// Bridges the stdio JSON-RPC layer → your existing Kafka APIs.
// Receives tool_name + json args from StdioTransport,
// calls the right Kafka operation, returns normalized JSON.
//
// This is the ONLY place that knows about both layers.
// StdioTransport knows nothing about Kafka.
// Kafka classes know nothing about MCP/JSON.
// ============================================================
class MCPDispatcher {
public:
    explicit MCPDispatcher(const ServerConfig& config);
    ~MCPDispatcher() = default;

    // Main dispatch — routes to the right handler
    DispatchResult dispatch(const std::string& tool_name,
                            const nlohmann::json& args);

private:
    // --- Per-tool handlers ---
    DispatchResult dispatch_consume_messages(const nlohmann::json& args);
    DispatchResult dispatch_produce_message(const nlohmann::json& args);
    DispatchResult dispatch_list_topics(const nlohmann::json& args);
    DispatchResult dispatch_describe_topic(const nlohmann::json& args);
    DispatchResult dispatch_list_consumer_groups(const nlohmann::json& args);
    DispatchResult dispatch_consumer_group_lag(const nlohmann::json& args);
    DispatchResult dispatch_cluster_metadata(const nlohmann::json& args);

    // --- Helpers ---
    DispatchResult ok(const nlohmann::json& result);
    DispatchResult err(const std::string& message);

    // --- Token efficiency: trim response if too large ---
    std::string trim_to_token_limit(const nlohmann::json& j);

    // --- Members ---
    ServerConfig                        config_;
    std::shared_ptr<KafkaConsumer>      consumer_;
    std::shared_ptr<KafkaProducer>      producer_;
    std::shared_ptr<KafkaAdmin>         admin_;
};
