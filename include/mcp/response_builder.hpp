#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "kafka/kafka_consumer.hpp"
#include "kafka/kafka_admin.hpp"
#include "mcp_service.pb.h"

// ============================================================
// ResponseBuilder
// Converts internal C++ types → token-efficient JSON strings
// for MCP ToolCallResponse.content
//
// Key principle: strip unnecessary fields, compact output,
// cap array sizes to control token usage.
// ============================================================
class ResponseBuilder {
public:
    ResponseBuilder() = default;

    // Build response from Kafka messages
    void build_consume_response(
        mcp::v1::ToolCallResponse& resp,
        const std::vector<KafkaMessageNormalized>& messages,
        const std::string& topic
    );

    // Build response from topic list
    void build_topics_response(
        mcp::v1::ToolCallResponse& resp,
        const std::vector<TopicMetadata>& topics
    );

    // Build response from consumer group lag
    void build_lag_response(
        mcp::v1::ToolCallResponse& resp,
        const std::vector<ConsumerGroupLag>& lag_info,
        const std::string& group_id
    );

    // Build error response
    void build_error_response(
        mcp::v1::ToolCallResponse& resp,
        const std::string& error_message
    );

    // Build broker metadata response
    void build_brokers_response(
        mcp::v1::ToolCallResponse& resp,
        const std::vector<BrokerMetadata>& brokers
    );

private:
    // Estimate token count (rough: 1 token ≈ 4 chars)
    int64_t estimate_tokens(const std::string& content);

    // Compact JSON to string (no whitespace)
    std::string compact(const nlohmann::json& j);
};
