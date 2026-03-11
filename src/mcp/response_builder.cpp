#include "mcp/response_builder.hpp"
#include "utils/logger.hpp"

#include <algorithm>

// ============================================================
// Helpers
// ============================================================

std::string ResponseBuilder::compact(const nlohmann::json& j) {
    return j.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

int64_t ResponseBuilder::estimate_tokens(const std::string& content) {
    return static_cast<int64_t>((content.size() + 3) / 4);
}

// ============================================================
// build_consume_response
// ============================================================
void ResponseBuilder::build_consume_response(
    mcp::v1::ToolCallResponse& resp,
    const std::vector<KafkaMessageNormalized>& messages,
    const std::string& topic)
{
    nlohmann::json out;
    out["topic"]   = topic;
    out["count"]   = messages.size();

    nlohmann::json arr = nlohmann::json::array();
    for (const auto& m : messages)
        arr.push_back(m.to_json());
    out["messages"] = std::move(arr);

    std::string content = compact(out);
    resp.set_content(content);
    resp.set_is_error(false);
    resp.set_token_count_estimate(estimate_tokens(content));

    LOG_DEBUG("build_consume_response: {} messages, ~{} tokens",
              messages.size(), resp.token_count_estimate());
}

// ============================================================
// build_topics_response
// ============================================================
void ResponseBuilder::build_topics_response(
    mcp::v1::ToolCallResponse& resp,
    const std::vector<TopicMetadata>& topics)
{
    nlohmann::json out;
    out["count"] = topics.size();

    nlohmann::json arr = nlohmann::json::array();
    for (const auto& t : topics)
        arr.push_back(t.to_json());
    out["topics"] = std::move(arr);

    std::string content = compact(out);
    resp.set_content(content);
    resp.set_is_error(false);
    resp.set_token_count_estimate(estimate_tokens(content));
}

// ============================================================
// build_lag_response
// ============================================================
void ResponseBuilder::build_lag_response(
    mcp::v1::ToolCallResponse& resp,
    const std::vector<ConsumerGroupLag>& lag_info,
    const std::string& group_id)
{
    nlohmann::json out;
    out["group_id"] = group_id;
    out["count"]    = lag_info.size();

    int64_t total_lag = 0;
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& l : lag_info) {
        arr.push_back(l.to_json());
        total_lag += l.lag;
    }
    out["total_lag"]    = total_lag;
    out["partitions"]   = std::move(arr);

    std::string content = compact(out);
    resp.set_content(content);
    resp.set_is_error(false);
    resp.set_token_count_estimate(estimate_tokens(content));
}

// ============================================================
// build_error_response
// ============================================================
void ResponseBuilder::build_error_response(
    mcp::v1::ToolCallResponse& resp,
    const std::string& error_message)
{
    nlohmann::json out;
    out["error"] = error_message;

    std::string content = compact(out);
    resp.set_content(content);
    resp.set_is_error(true);
    resp.set_token_count_estimate(estimate_tokens(content));

    LOG_WARN("build_error_response: {}", error_message);
}

// ============================================================
// build_brokers_response
// ============================================================
void ResponseBuilder::build_brokers_response(
    mcp::v1::ToolCallResponse& resp,
    const std::vector<BrokerMetadata>& brokers)
{
    nlohmann::json out;
    out["broker_count"] = brokers.size();

    nlohmann::json arr = nlohmann::json::array();
    for (const auto& b : brokers)
        arr.push_back(b.to_json());
    out["brokers"] = std::move(arr);

    std::string content = compact(out);
    resp.set_content(content);
    resp.set_is_error(false);
    resp.set_token_count_estimate(estimate_tokens(content));
}
