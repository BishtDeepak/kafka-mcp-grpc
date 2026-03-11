#include "kafka/kafka_consumer.hpp"
#include "utils/logger.hpp"

#include <stdexcept>
#include <chrono>

// ============================================================
// KafkaMessageNormalized
// ============================================================

nlohmann::json KafkaMessageNormalized::to_json() const {
    nlohmann::json j;
    j["topic"]        = topic;
    j["partition"]    = partition;
    j["offset"]       = offset;
    j["timestamp_ms"] = timestamp_ms;
    if (!key.empty())
        j["key"] = key;
    j["value"] = value;
    if (!headers.empty())
        j["headers"] = headers;
    return j;
}

// ============================================================
// KafkaConsumer
// ============================================================

KafkaConsumer::KafkaConsumer(const KafkaConfig& config)
    : config_(config) {}

KafkaConsumer::~KafkaConsumer() {
    disconnect();
}

std::unique_ptr<RdKafka::Conf> KafkaConsumer::build_rdkafka_conf() {
    std::string errstr;
    auto conf = std::unique_ptr<RdKafka::Conf>(
        RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));

    auto set = [&](const std::string& key, const std::string& val) {
        if (conf->set(key, val, errstr) != RdKafka::Conf::CONF_OK)
            throw std::runtime_error("rdkafka conf error [" + key + "]: " + errstr);
    };

    set("bootstrap.servers", config_.brokers);
    set("security.protocol", config_.security_protocol);

    if (!config_.sasl_mechanism.empty()) {
        set("sasl.mechanism",  config_.sasl_mechanism);
        set("sasl.username",   config_.sasl_username);
        set("sasl.password",   config_.sasl_password);
    }

    set("fetch.message.max.bytes", std::to_string(config_.fetch_max_bytes));
    set("session.timeout.ms",      std::to_string(config_.session_timeout_ms));
    set("group.id",                config_.default_group_id);
    set("auto.offset.reset",       "earliest");
    set("enable.auto.commit",      "false");

    return conf;
}

bool KafkaConsumer::connect() {
    if (connected_) return true;
    try {
        auto conf = build_rdkafka_conf();
        std::string errstr;
        consumer_.reset(RdKafka::Consumer::create(conf.get(), errstr));
        if (!consumer_) {
            LOG_ERROR("Failed to create Kafka consumer: {}", errstr);
            return false;
        }
        connected_ = true;
        LOG_INFO("KafkaConsumer connected to brokers: {}", config_.brokers);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("KafkaConsumer connect error: {}", e.what());
        return false;
    }
}

void KafkaConsumer::disconnect() {
    if (consumer_) {
        consumer_ = nullptr;
    }
    if (group_consumer_) {
        group_consumer_->close();
        group_consumer_ = nullptr;
    }
    connected_ = false;
    LOG_INFO("KafkaConsumer disconnected");
}

bool KafkaConsumer::is_connected() const {
    return connected_;
}

std::vector<KafkaMessageNormalized> KafkaConsumer::consume(
    const std::string& topic,
    int32_t partition,
    int64_t start_offset,
    int32_t max_messages,
    int32_t timeout_ms)
{
    std::vector<KafkaMessageNormalized> results;

    if (!connected_ && !connect()) {
        LOG_ERROR("consume: not connected");
        return results;
    }

    std::string errstr;
    auto* topic_conf = RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC);
    auto* rd_topic   = RdKafka::Topic::create(consumer_.get(), topic, topic_conf, errstr);
    delete topic_conf;

    if (!rd_topic) {
        LOG_ERROR("Failed to create topic handle for '{}': {}", topic, errstr);
        return results;
    }

    int64_t offset = (start_offset == RdKafka::Topic::OFFSET_END)
                   ? RdKafka::Topic::OFFSET_END
                   : start_offset;

    if (consumer_->start(rd_topic, partition, offset) != RdKafka::ERR_NO_ERROR) {
        LOG_ERROR("Failed to start consumer on topic '{}' partition {}", topic, partition);
        delete rd_topic;
        return results;
    }

    for (int i = 0; i < max_messages; ++i) {
        auto* msg = consumer_->consume(rd_topic, partition, timeout_ms);
        if (!msg) break;

        if (msg->err() == RdKafka::ERR__TIMED_OUT ||
            msg->err() == RdKafka::ERR__PARTITION_EOF) {
            delete msg;
            break;
        }

        if (msg->err() != RdKafka::ERR_NO_ERROR) {
            LOG_WARN("consume error: {}", msg->errstr());
            delete msg;
            continue;
        }

        KafkaMessageNormalized m;
        m.topic     = msg->topic_name();
        m.partition = msg->partition();
        m.offset    = msg->offset();
        m.timestamp_ms = msg->timestamp().timestamp;
        if (msg->key())
            m.key = *msg->key();
        if (msg->payload())
            m.value = std::string(static_cast<const char*>(msg->payload()), msg->len());

        results.push_back(std::move(m));
        delete msg;
    }

    consumer_->stop(rd_topic, partition);
    delete rd_topic;

    LOG_DEBUG("consume: got {} messages from topic '{}'", results.size(), topic);
    return results;
}

std::vector<KafkaMessageNormalized> KafkaConsumer::consume_group(
    const std::string& topic,
    const std::string& group_id,
    int32_t max_messages,
    int32_t timeout_ms)
{
    std::vector<KafkaMessageNormalized> results;

    // Build a fresh group consumer config with the requested group_id
    try {
        std::string errstr;
        auto conf = build_rdkafka_conf();
        conf->set("group.id", group_id, errstr);

        group_consumer_.reset(RdKafka::KafkaConsumer::create(conf.get(), errstr));
        if (!group_consumer_) {
            LOG_ERROR("Failed to create group consumer: {}", errstr);
            return results;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("consume_group conf error: {}", e.what());
        return results;
    }

    if (group_consumer_->subscribe({topic}) != RdKafka::ERR_NO_ERROR) {
        LOG_ERROR("Failed to subscribe to topic '{}' with group '{}'", topic, group_id);
        return results;
    }

    for (int i = 0; i < max_messages; ++i) {
        auto* msg = group_consumer_->consume(timeout_ms);
        if (!msg) break;

        if (msg->err() == RdKafka::ERR__TIMED_OUT ||
            msg->err() == RdKafka::ERR__PARTITION_EOF) {
            delete msg;
            break;
        }

        if (msg->err() != RdKafka::ERR_NO_ERROR) {
            LOG_WARN("consume_group error: {}", msg->errstr());
            delete msg;
            continue;
        }

        KafkaMessageNormalized m;
        m.topic     = msg->topic_name();
        m.partition = msg->partition();
        m.offset    = msg->offset();
        m.timestamp_ms = msg->timestamp().timestamp;
        if (msg->key())
            m.key = *msg->key();
        if (msg->payload())
            m.value = std::string(static_cast<const char*>(msg->payload()), msg->len());

        results.push_back(std::move(m));
        delete msg;
    }

    group_consumer_->close();
    group_consumer_ = nullptr;

    LOG_DEBUG("consume_group: got {} messages from topic '{}' group '{}'",
              results.size(), topic, group_id);
    return results;
}
