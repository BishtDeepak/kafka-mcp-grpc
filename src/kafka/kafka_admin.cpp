#include "kafka/kafka_admin.hpp"
#include "utils/logger.hpp"

#include <stdexcept>

// ============================================================
// Metadata types — to_json()
// ============================================================

nlohmann::json TopicMetadata::to_json() const {
    return {
        {"name",               name},
        {"partitions",         partition_count},
        {"replication_factor", replication_factor}
    };
}

nlohmann::json BrokerMetadata::to_json() const {
    return {
        {"id",            broker_id},
        {"host",          host},
        {"port",          port},
        {"is_controller", is_controller}
    };
}

nlohmann::json ConsumerGroupLag::to_json() const {
    return {
        {"topic",          topic},
        {"partition",      partition},
        {"current_offset", current_offset},
        {"log_end_offset", log_end_offset},
        {"lag",            lag}
    };
}

// ============================================================
// KafkaAdmin
// ============================================================

KafkaAdmin::KafkaAdmin(const KafkaConfig& config)
    :    config_(config) {
}

KafkaAdmin::~KafkaAdmin() {
    disconnect();
}

std::unique_ptr<RdKafka::Conf> KafkaAdmin::build_rdkafka_conf() {
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
    return conf;
}

bool KafkaAdmin::connect() {
    if (connected_) {
        return true;
    }
    try {
        auto conf = build_rdkafka_conf();
        std::string errstr;
        // Admin operations are performed via a lightweight producer handle
        admin_client_.reset(RdKafka::Producer::create(conf.get(), errstr));
        if (!admin_client_) {
            LOG_ERROR("Failed to create admin client: {}", errstr);
            return false;
        }
        connected_ = true;
        LOG_INFO("KafkaAdmin connected to brokers: {}", config_.brokers);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("KafkaAdmin connect error: {}", e.what());
        return false;
    }
}

void KafkaAdmin::disconnect() {
    admin_client_ = nullptr;
    connected_ = false;
    LOG_INFO("KafkaAdmin disconnected");
}

bool KafkaAdmin::is_connected() const {
    return connected_;
}

// Helper: fetch cluster metadata from broker
static RdKafka::Metadata* fetch_metadata(
    RdKafka::Producer* client,
    const std::string& topic = "",
    int timeoutMs = 5000) {
    RdKafka::Metadata* metaDataP = nullptr;
    RdKafka::ErrorCode err;
    if (topic.empty()) {
        err = client->metadata(true, nullptr, &metaDataP, timeoutMs);
    } else {
        std::string errstr;
        // 1. Create a temporary topic configuration
        std::unique_ptr<RdKafka::Conf> topicConf(RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC));
        
        // 2. Create the Topic handle (This is what metadata() actually needs)
        std::unique_ptr<RdKafka::Topic> topicHandle(
            RdKafka::Topic::create(client, topic, topicConf.get(), errstr)
        );
        if (!topicHandle) {
            LOG_ERROR("Failed to create topic handle for metadata: {}", errstr);
            return nullptr;
        }

        // 3. Pass the topic handle using .get()
        err = client->metadata(false, topicHandle.get(), &metaDataP, timeoutMs);
    }
    if (err != RdKafka::ERR_NO_ERROR) {
        LOG_ERROR("metadata fetch failed: {}", RdKafka::err2str(err));
        return nullptr;
    }
    return metaDataP;
}

std::vector<TopicMetadata> KafkaAdmin::listTopics() {
    std::vector<TopicMetadata> result;
    if (!connected_ && !connect()) {
        return result;
    }
    auto* mdP = fetch_metadata(admin_client_.get());
    if (!mdP) {
        return result;
    }
    for (const auto* topicP : *mdP->topics()) {
        if (topicP->err() != RdKafka::ERR_NO_ERROR) continue;
        TopicMetadata tm;
        tm.name            = topicP->topic();
        tm.partition_count = static_cast<int32_t>(topicP->partitions()->size());
        // replication_factor: read from first partition's replicas
        tm.replication_factor = tm.partition_count > 0
            ? static_cast<int32_t>((*topicP->partitions())[0]->replicas()->size())
            : 0;
        result.emplace_back(std::move(tm));
    }
    delete mdP;
    LOG_DEBUG("listTopics: found {} topics", result.size());
    return result;
}

TopicMetadata KafkaAdmin::describe_topic(const std::string& topicStr) {
    if (!connected_ && !connect()) {
        return {};
    }
    auto* mdP = fetch_metadata(admin_client_.get(), topicStr);
    if (!mdP) {
        return {};
    }
    TopicMetadata tm;
    for (const auto* topicP : *mdP->topics()) {
        if (topicP->topic() != topicStr) continue;
        tm.name            = topicP->topic();
        tm.partition_count = static_cast<int32_t>(topicP->partitions()->size());
        tm.replication_factor = tm.partition_count > 0
            ? static_cast<int32_t>((*topicP->partitions())[0]->replicas()->size())
            : 0;
        break;
    }
    delete mdP;
    return tm;
}

std::vector<std::string> KafkaAdmin::list_consumer_groups() {
    // librdkafka 1.6 does not have a high-level ListConsumerGroups admin API.
    // Return an informative placeholder; upgrade to 2.x for full support.
    LOG_WARN("list_consumer_groups: requires librdkafka >= 2.0. "
             "Current install is 1.6. Returning empty list.");
    return {};
}

std::vector<ConsumerGroupLag> KafkaAdmin::get_consumer_group_lag(
    const std::string& group_id,
    const std::string& topic)
{
    // Full consumer group lag requires AdminClient::ListConsumerGroupOffsets
    // (available in librdkafka >= 2.0). With 1.6 we can only approximate
    // by comparing committed vs. high-water-mark offsets using a consumer handle.
    LOG_WARN("get_consumer_group_lag: limited support in librdkafka 1.6. "
             "group={} topic={}", group_id, topic.empty() ? "(all)" : topic);

    std::vector<ConsumerGroupLag> result;
    // Stub — returns empty. Upgrade rdkafka to 2.x for full implementation.
    return result;
}

std::vector<BrokerMetadata> KafkaAdmin::get_brokers() {
    std::vector<BrokerMetadata> result;
    if (!connected_ && !connect()) {
        return result;
    }
    auto* mdP = fetch_metadata(admin_client_.get());
    if (!mdP) {
        return result;
    }
    int32_t controller_id = mdP->orig_broker_id();

    for (const auto* broker : *mdP->brokers()) {
        BrokerMetadata bm;
        bm.broker_id     = broker->id();
        bm.host          = broker->host();
        bm.port          = broker->port();
        bm.is_controller = (broker->id() == controller_id);
        result.emplace_back(std::move(bm));
    }

    delete mdP;
    LOG_DEBUG("get_brokers: found {} brokers", result.size());
    return result;
}
