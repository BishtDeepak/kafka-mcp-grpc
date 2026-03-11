#pragma once

#include <string>
#include <vector>
#include <memory>

#include <librdkafka/rdkafkacpp.h>
#include <nlohmann/json.hpp>

#include "utils/config_loader.hpp"

// ============================================================
// KafkaMessage — normalized message structure
// This is our internal C++ representation.
// Protobuf types are used only at the gRPC boundary.
// ============================================================
struct KafkaMessageNormalized {
    std::string topic;
    int32_t     partition;
    int64_t     offset;
    std::string key;
    std::string value;
    int64_t     timestamp_ms;
    std::unordered_map<std::string, std::string> headers;

    // Serialize to JSON for MCP response (token-efficient format)
    nlohmann::json to_json() const;
};

// ============================================================
// KafkaConsumer
// Wraps librdkafka consumer with token-efficient reads
// ============================================================
class KafkaConsumer {
public:
    explicit KafkaConsumer(const KafkaConfig& config);
    ~KafkaConsumer();

    // Non-copyable
    KafkaConsumer(const KafkaConsumer&) = delete;
    KafkaConsumer& operator=(const KafkaConsumer&) = delete;

    bool connect();
    void disconnect();
    bool is_connected() const;

    // Consume messages from a topic/partition
    // max_messages limits token usage — default 10
    std::vector<KafkaMessageNormalized> consume(
        const std::string& topic,
        int32_t            partition    = RdKafka::Topic::PARTITION_UA,
        int64_t            start_offset = RdKafka::Topic::OFFSET_END,
        int32_t            max_messages = 10,
        int32_t            timeout_ms   = 5000
    );

    // Consume with consumer group
    std::vector<KafkaMessageNormalized> consume_group(
        const std::string& topic,
        const std::string& group_id,
        int32_t            max_messages = 10,
        int32_t            timeout_ms   = 5000
    );

private:
    KafkaConfig                             config_;
    std::unique_ptr<RdKafka::Consumer>      consumer_;
    std::unique_ptr<RdKafka::KafkaConsumer> group_consumer_;
    bool                                    connected_{false};

    // Build rdkafka config from our ServerConfig
    std::unique_ptr<RdKafka::Conf> build_rdkafka_conf();
};
