#include "kafka/kafka_producer.hpp"
#include "utils/logger.hpp"

#include <stdexcept>

// ============================================================
// Delivery report callback — logs produce results
// ============================================================
class DeliveryReportCb : public RdKafka::DeliveryReportCb {
public:
    void dr_cb(RdKafka::Message& msg) override {
        if (msg.err() != RdKafka::ERR_NO_ERROR) {
            LOG_ERROR("Produce delivery failed for topic '{}': {}",
                      msg.topic_name(), msg.errstr());
        } else {
            LOG_DEBUG("Produced to topic '{}' partition {} offset {}",
                      msg.topic_name(), msg.partition(), msg.offset());
        }
    }
};

static DeliveryReportCb g_dr_cb;

// ============================================================
// KafkaProducer
// ============================================================

KafkaProducer::KafkaProducer(const KafkaConfig& config)
    : config_(config) {}

KafkaProducer::~KafkaProducer() {
    disconnect();
}

std::unique_ptr<RdKafka::Conf> KafkaProducer::build_rdkafka_conf() {
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
        set("sasl.mechanism", config_.sasl_mechanism);
        set("sasl.username",  config_.sasl_username);
        set("sasl.password",  config_.sasl_password);
    }

    if (conf->set("dr_cb", &g_dr_cb, errstr) != RdKafka::Conf::CONF_OK)
        throw std::runtime_error("Failed to set delivery report callback: " + errstr);

    // Reasonable defaults for MCP use — low latency over throughput
    set("queue.buffering.max.ms", "5");
    set("acks", "1");

    return conf;
}

bool KafkaProducer::connect() {
    if (connected_) return true;
    try {
        auto conf = build_rdkafka_conf();
        std::string errstr;
        producer_.reset(RdKafka::Producer::create(conf.get(), errstr));
        if (!producer_) {
            LOG_ERROR("Failed to create Kafka producer: {}", errstr);
            return false;
        }
        connected_ = true;
        LOG_INFO("KafkaProducer connected to brokers: {}", config_.brokers);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("KafkaProducer connect error: {}", e.what());
        return false;
    }
}

void KafkaProducer::disconnect() {
    if (producer_) {
        flush(5000);
        producer_ = nullptr;
    }
    connected_ = false;
    LOG_INFO("KafkaProducer disconnected");
}

bool KafkaProducer::is_connected() const {
    return connected_;
}

ProduceResult KafkaProducer::produce(
    const std::string& topic,
    const std::string& value,
    const std::string& key,
    int32_t partition,
    const std::unordered_map<std::string, std::string>& headers)
{
    ProduceResult result;
    result.topic = topic;
    result.partition = partition;
    result.offset = -1;

    if (!connected_ && !connect()) {
        result.success = false;
        result.error_message = "Not connected to Kafka";
        return result;
    }

    // Build headers
    std::unique_ptr<RdKafka::Headers> rd_headers;
    if (!headers.empty()) {
        rd_headers.reset(RdKafka::Headers::create());
        for (const auto& [k, v] : headers)
            rd_headers->add(k, v.data(), v.size());
    }

    const void* key_ptr  = key.empty()   ? nullptr : key.data();
    size_t      key_len  = key.empty()   ? 0       : key.size();

    RdKafka::ErrorCode err = producer_->produce(
        topic,
        partition,
        RdKafka::Producer::RK_MSG_COPY,
        const_cast<char*>(value.data()), value.size(),
        key_ptr, key_len,
        0,                           // timestamp (0 = now)
        rd_headers.release(),        // rdkafka takes ownership
        nullptr                      // opaque
    );

    if (err != RdKafka::ERR_NO_ERROR) {
        result.success = false;
        result.error_message = RdKafka::err2str(err);
        LOG_ERROR("Produce failed for topic '{}': {}", topic, result.error_message);
    } else {
        result.success = true;
        producer_->poll(0); // trigger delivery callbacks
        LOG_DEBUG("Produce queued for topic '{}'", topic);
    }

    return result;
}

void KafkaProducer::flush(int timeout_ms) {
    if (producer_)
        producer_->flush(timeout_ms);
}
