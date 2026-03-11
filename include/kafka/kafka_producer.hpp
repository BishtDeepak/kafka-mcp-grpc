#pragma once

#include <string>
#include <unordered_map>
#include <memory>

#include <librdkafka/rdkafkacpp.h>

#include "utils/config_loader.hpp"

// ============================================================
// ProduceResult — result of a produce operation
// ============================================================
struct ProduceResult {
    bool        success;
    std::string topic;
    int32_t     partition;
    int64_t     offset;
    std::string error_message;
};

// ============================================================
// KafkaProducer
// Wraps librdkafka producer
// ============================================================
class KafkaProducer {
public:
    explicit KafkaProducer(const KafkaConfig& config);
    ~KafkaProducer();

    KafkaProducer(const KafkaProducer&) = delete;
    KafkaProducer& operator=(const KafkaProducer&) = delete;

    bool connect();
    void disconnect();
    bool is_connected() const;

    // Produce a single message
    ProduceResult produce(
        const std::string& topic,
        const std::string& value,
        const std::string& key     = "",
        int32_t            partition = RdKafka::Topic::PARTITION_UA,
        const std::unordered_map<std::string, std::string>& headers = {}
    );

    // Flush pending messages
    void flush(int timeout_ms = 5000);

private:
    KafkaConfig                         config_;
    std::unique_ptr<RdKafka::Producer>  producer_;
    bool                                connected_{false};

    std::unique_ptr<RdKafka::Conf> build_rdkafka_conf();
};
