#include "mcp/mcp_dispatcher.hpp"
#include "utils/logger.hpp"

#include <nlohmann/json.hpp>
#include <sstream>
#include <algorithm>

using json = nlohmann::json;

// ============================================================
// Constructor — initialize Kafka clients
// ============================================================
MCPDispatcher::MCPDispatcher(const ServerConfig& config)
    : config_(config)
    , consumer_(std::make_shared<KafkaConsumer>(config.kafka))
    , producer_(std::make_shared<KafkaProducer>(config.kafka))
    , admin_(std::make_shared<KafkaAdmin>(config.kafka))
{
    consumer_->connect();
    producer_->connect();
    admin_->connect();
}

// ============================================================
// dispatch()
// Routes tool_name → the right handler.
// Matches your existing tool_registry_ names exactly.
// ============================================================
DispatchResult MCPDispatcher::dispatch(const std::string& tool_name,
                                       const json& args) {
    LOG_DEBUG("MCPDispatcher: dispatching tool='{}'", tool_name);

    if (tool_name == "consume_messages")     return dispatch_consume_messages(args);
    if (tool_name == "produce_message")      return dispatch_produce_message(args);
    if (tool_name == "list_topics")          return dispatch_list_topics(args);
    if (tool_name == "describe_topic")       return dispatch_describe_topic(args);
    if (tool_name == "list_consumer_groups") return dispatch_list_consumer_groups(args);
    if (tool_name == "consumer_group_lag")   return dispatch_consumer_group_lag(args);
    if (tool_name == "cluster_metadata")     return dispatch_cluster_metadata(args);

    return err("Unknown tool: " + tool_name);
}

// ============================================================
// dispatch_consume_messages()
// ============================================================
DispatchResult MCPDispatcher::dispatch_consume_messages(const json& args) {
    if (!args.contains("topic")) {
        return err("Missing required parameter: topic");
    }

    std::string topic    = args["topic"].get<std::string>();
    int32_t partition    = args.value("partition",    -1);
    int64_t offset       = args.value("offset",       (int64_t)-1);
    int32_t max_messages = args.value("max_messages", config_.max_messages_per_request);
    int32_t timeout_ms   = args.value("timeout_ms",   5000);

    // Cap max_messages for token efficiency
    max_messages = std::min(max_messages, config_.max_messages_per_request);

    LOG_INFO("MCPDispatcher: consume topic={} partition={} max={}",
             topic, partition, max_messages);

    try {
        auto messages = consumer_->consume(topic, partition, offset,
                                           max_messages, timeout_ms);

        json result = {
            {"topic",         topic},
            {"message_count", (int)messages.size()},
            {"messages",      json::array()}
        };

        for (const auto& msg : messages) {
            result["messages"].push_back(msg.to_json());
        }

        return ok(result);

    } catch (const std::exception& e) {
        return err("Kafka consume error: " + std::string(e.what()));
    }
}

// ============================================================
// dispatch_produce_message()
// ============================================================
DispatchResult MCPDispatcher::dispatch_produce_message(const json& args) {
    if (!args.contains("topic") || !args.contains("value")) {
        return err("Missing required parameters: topic, value");
    }

    std::string topic     = args["topic"].get<std::string>();
    std::string value     = args["value"].get<std::string>();
    std::string key       = args.value("key", std::string(""));
    int32_t     partition = args.value("partition", -1);

    LOG_INFO("MCPDispatcher: produce topic={} key={}", topic, key);

    try {
        auto result = producer_->produce(topic, value, key, partition);

        json resp = {
            {"success",   result.success},
            {"topic",     result.topic},
            {"partition", result.partition},
            {"offset",    result.offset}
        };

        if (!result.success) {
            return err("Produce failed: " + result.error_message);
        }

        return ok(resp);

    } catch (const std::exception& e) {
        return err("Kafka produce error: " + std::string(e.what()));
    }
}

// ============================================================
// dispatch_list_topics()
// ============================================================
DispatchResult MCPDispatcher::dispatch_list_topics(const json& args) {
    std::string filter = args.value("filter", std::string(""));

    LOG_INFO("MCPDispatcher: list_topics filter='{}'", filter);

    try {
        auto topics = admin_->list_topics();

        json result = {
            {"topic_count", (int)topics.size()},
            {"topics",      json::array()}
        };

        for (const auto& t : topics) {
            // Apply filter if provided
            if (!filter.empty() &&
                t.name.find(filter) == std::string::npos) {
                continue;
            }
            result["topics"].push_back(t.to_json());
        }

        return ok(result);

    } catch (const std::exception& e) {
        return err("Kafka list_topics error: " + std::string(e.what()));
    }
}

// ============================================================
// dispatch_describe_topic()
// ============================================================
DispatchResult MCPDispatcher::dispatch_describe_topic(const json& args) {
    if (!args.contains("topic")) {
        return err("Missing required parameter: topic");
    }

    std::string topic = args["topic"].get<std::string>();
    LOG_INFO("MCPDispatcher: describe_topic topic={}", topic);

    try {
        auto metadata = admin_->describe_topic(topic);
        return ok(metadata.to_json());

    } catch (const std::exception& e) {
        return err("Kafka describe_topic error: " + std::string(e.what()));
    }
}

// ============================================================
// dispatch_list_consumer_groups()
// ============================================================
DispatchResult MCPDispatcher::dispatch_list_consumer_groups(const json& /*args*/) {
    LOG_INFO("MCPDispatcher: list_consumer_groups");

    try {
        auto groups = admin_->list_consumer_groups();

        json result = {
            {"group_count", (int)groups.size()},
            {"groups",      groups}
        };

        return ok(result);

    } catch (const std::exception& e) {
        return err("Kafka list_consumer_groups error: " + std::string(e.what()));
    }
}

// ============================================================
// dispatch_consumer_group_lag()
// ============================================================
DispatchResult MCPDispatcher::dispatch_consumer_group_lag(const json& args) {
    if (!args.contains("group_id")) {
        return err("Missing required parameter: group_id");
    }

    std::string group_id = args["group_id"].get<std::string>();
    std::string topic    = args.value("topic", std::string(""));

    LOG_INFO("MCPDispatcher: consumer_group_lag group={} topic={}",
             group_id, topic);

    try {
        auto lag_info = admin_->get_consumer_group_lag(group_id, topic);

        int64_t total_lag = 0;
        json lag_array = json::array();

        for (const auto& lag : lag_info) {
            lag_array.push_back(lag.to_json());
            total_lag += lag.lag;
        }

        json result = {
            {"group_id",   group_id},
            {"total_lag",  total_lag},
            {"partition_count", (int)lag_info.size()},
            {"partitions", lag_array}
        };

        return ok(result);

    } catch (const std::exception& e) {
        return err("Kafka consumer_group_lag error: " + std::string(e.what()));
    }
}

// ============================================================
// dispatch_cluster_metadata()
// ============================================================
DispatchResult MCPDispatcher::dispatch_cluster_metadata(const json& /*args*/) {
    LOG_INFO("MCPDispatcher: cluster_metadata");

    try {
        auto brokers = admin_->get_brokers();

        json broker_array = json::array();
        for (const auto& b : brokers) {
            broker_array.push_back(b.to_json());
        }

        json result = {
            {"broker_count", (int)brokers.size()},
            {"brokers",      broker_array}
        };

        return ok(result);

    } catch (const std::exception& e) {
        return err("Kafka cluster_metadata error: " + std::string(e.what()));
    }
}

// ============================================================
// Helpers
// ============================================================
DispatchResult MCPDispatcher::ok(const json& result) {
    std::string content = trim_to_token_limit(result);
    return {true, content, ""};
}

DispatchResult MCPDispatcher::err(const std::string& message) {
    LOG_WARN("MCPDispatcher: error — {}", message);
    return {false, "", message};
}

// Compact JSON + cap at max_response_size_bytes for token efficiency
std::string MCPDispatcher::trim_to_token_limit(const json& j) {
    std::string content = j.dump(); // compact — no whitespace

    if ((int)content.size() > config_.max_response_size_bytes) {
        LOG_WARN("MCPDispatcher: response too large ({} bytes), trimming to {}",
                 content.size(), config_.max_response_size_bytes);
        content = content.substr(0, config_.max_response_size_bytes);
        content += "... [truncated for token efficiency]";
    }

    return content;
}