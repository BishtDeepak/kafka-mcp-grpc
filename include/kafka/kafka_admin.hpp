#pragma once

#include <string>
#include <vector>
#include <memory>

#include <librdkafka/rdkafkacpp.h>
#include <nlohmann/json.hpp>

#include "utils/config_loader.hpp"

// ============================================================
// Normalized admin data types (internal C++ representation)
// ============================================================

struct TopicMetadata {
    std::string name;
    int32_t     partition_count;
    int32_t     replication_factor;
    nlohmann::json to_json() const;
};

struct BrokerMetadata {
    int32_t     broker_id;
    std::string host;
    int32_t     port;
    bool        is_controller;
    nlohmann::json to_json() const;
};

struct ConsumerGroupLag {
    std::string topic;
    int32_t     partition;
    int64_t     current_offset;
    int64_t     log_end_offset;
    int64_t     lag;
    nlohmann::json to_json() const;
};

// ============================================================
// KafkaAdmin
// Cluster metadata, topic info, consumer group lag
// ============================================================
class KafkaAdmin {
public:
    explicit KafkaAdmin(const KafkaConfig& config);
    ~KafkaAdmin();

    KafkaAdmin(const KafkaAdmin&) = delete;
    KafkaAdmin& operator=(const KafkaAdmin&) = delete;

    bool connect();
    void disconnect();
    bool is_connected() const;

    // List all topics
    std::vector<TopicMetadata> listTopics();

    // Describe a specific topic
    TopicMetadata describe_topic(const std::string& topic_name);

    // List consumer groups
    std::vector<std::string> list_consumer_groups();

    // Get consumer group lag
    std::vector<ConsumerGroupLag> get_consumer_group_lag(
        const std::string& group_id,
        const std::string& topic = ""  // empty = all topics
    );

    // Get broker/cluster metadata
    std::vector<BrokerMetadata> get_brokers();

private:
    KafkaConfig                         config_;
    std::unique_ptr<RdKafka::Producer>  admin_client_; // Admin ops via producer handle
    bool                                connected_{false};

    std::unique_ptr<RdKafka::Conf> build_rdkafka_conf();
};
