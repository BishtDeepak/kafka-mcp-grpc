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
    : config_(config) {}

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
    if (connected_) return true;
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
static RdKafka::Metadata* fetch_metadata(RdKafka::Producer* client,
                                          const std::string& topic = "",
                                          int timeout_ms = 5000) {
    RdKafka::Metadata* md = nullptr;
    RdKafka::ErrorCode err;

    if (topic.empty()) {
        err = client->metadata(true, nullptr, &md, timeout_ms);
    } else {
        // Create a temporary topic handle to scope metadata fetch
        std::string errstr;
        auto* tconf = RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC);
        auto* th = RdKafka::Topic::create(client, topic, tconf, errstr);
        delete tconf;
        if (!th) return nullptr;
        err = client->metadata(false, th, &md, timeout_ms);
        delete th;
    }

    if (err != RdKafka::ERR_NO_ERROR) {
        LOG_ERROR("metadata fetch failed: {}", RdKafka::err2str(err));
        return nullptr;
    }
    return md;
}

std::vector<TopicMetadata> KafkaAdmin::list_topics() {
    std::vector<TopicMetadata> result;
    if (!connected_ && !connect()) return result;

    auto* md = fetch_metadata(admin_client_.get());
    if (!md) return result;

    for (const auto* t : *md->topics()) {
        if (t->err() != RdKafka::ERR_NO_ERROR) continue;
        TopicMetadata tm;
        tm.name            = t->topic();
        tm.partition_count = static_cast<int32_t>(t->partitions()->size());
        // replication_factor: read from first partition's replicas
        tm.replication_factor = tm.partition_count > 0
            ? static_cast<int32_t>((*t->partitions())[0]->replicas()->size())
            : 0;
        result.push_back(std::move(tm));
    }

    delete md;
    LOG_DEBUG("list_topics: found {} topics", result.size());
    return result;
}

TopicMetadata KafkaAdmin::describe_topic(const std::string& topic_name) {
    if (!connected_ && !connect()) return {};

    auto* md = fetch_metadata(admin_client_.get(), topic_name);
    if (!md) return {};

    TopicMetadata tm;
    for (const auto* t : *md->topics()) {
        if (t->topic() != topic_name) continue;
        tm.name            = t->topic();
        tm.partition_count = static_cast<int32_t>(t->partitions()->size());
        tm.replication_factor = tm.partition_count > 0
            ? static_cast<int32_t>((*t->partitions())[0]->replicas()->size())
            : 0;
        break;
    }
    delete md;
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
    if (!connected_ && !connect()) return result;

    auto* md = fetch_metadata(admin_client_.get());
    if (!md) return result;

    int32_t controller_id = md->orig_broker_id();

    for (const auto* b : *md->brokers()) {
        BrokerMetadata bm;
        bm.broker_id     = b->id();
        bm.host          = b->host();
        bm.port          = b->port();
        bm.is_controller = (b->id() == controller_id);
        result.push_back(std::move(bm));
    }

    delete md;
    LOG_DEBUG("get_brokers: found {} brokers", result.size());
    return result;
}
