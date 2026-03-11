#include "utils/config_loader.hpp"

#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

// Helper: read a string from json with a default
static std::string jstr(const json& j, const std::string& key,
                         const std::string& def = "") {
    return j.contains(key) && j[key].is_string() ? j[key].get<std::string>() : def;
}

static int32_t jint(const json& j, const std::string& key, int32_t def = 0) {
    return j.contains(key) && j[key].is_number() ? j[key].get<int32_t>() : def;
}

std::optional<ServerConfig> ConfigLoader::load(const std::string& config_file) {
    std::ifstream f(config_file);
    if (!f.is_open()) {
        spdlog::error("ConfigLoader: cannot open '{}'", config_file);
        return std::nullopt;
    }

    json root;
    try {
        f >> root;
    } catch (const json::parse_error& e) {
        spdlog::error("ConfigLoader: JSON parse error in '{}': {}", config_file, e.what());
        return std::nullopt;
    }

    ServerConfig cfg = defaults();

    // gRPC
    if (root.contains("grpc")) {
        const auto& g = root["grpc"];
        cfg.grpc_listen_address = jstr(g, "listen_address", cfg.grpc_listen_address);
    }

    // Kafka
    if (root.contains("kafka")) {
        const auto& k = root["kafka"];
        cfg.kafka.brokers           = jstr(k, "brokers",           cfg.kafka.brokers);
        cfg.kafka.security_protocol = jstr(k, "security_protocol", cfg.kafka.security_protocol);
        cfg.kafka.sasl_mechanism    = jstr(k, "sasl_mechanism",    cfg.kafka.sasl_mechanism);
        cfg.kafka.sasl_username     = jstr(k, "sasl_username",     cfg.kafka.sasl_username);
        cfg.kafka.sasl_password     = jstr(k, "sasl_password",     cfg.kafka.sasl_password);
        cfg.kafka.default_group_id  = jstr(k, "default_group_id",  cfg.kafka.default_group_id);
        cfg.kafka.fetch_max_bytes   = jint(k, "fetch_max_bytes",   cfg.kafka.fetch_max_bytes);
        cfg.kafka.session_timeout_ms = jint(k, "session_timeout_ms", cfg.kafka.session_timeout_ms);
    }
    cfg.kafka_brokers = cfg.kafka.brokers;

    // Logging
    if (root.contains("logging")) {
        const auto& l = root["logging"];
        cfg.log_level = jstr(l, "level", cfg.log_level);
        cfg.log_file  = jstr(l, "file",  cfg.log_file);
    }

    // Token efficiency
    if (root.contains("limits")) {
        const auto& lim = root["limits"];
        cfg.max_messages_per_request = jint(lim, "max_messages_per_request",
                                            cfg.max_messages_per_request);
        cfg.max_response_size_bytes  = jint(lim, "max_response_size_bytes",
                                            cfg.max_response_size_bytes);
    }

    spdlog::info("ConfigLoader: loaded config from '{}'", config_file);
    return cfg;
}

ServerConfig ConfigLoader::defaults() {
    ServerConfig cfg;
    // All defaults are already set by struct initialisers in the header.
    return cfg;
}
