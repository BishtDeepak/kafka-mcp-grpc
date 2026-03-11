#pragma once

#include <string>
#include <optional>

// ============================================================
// KafkaConfig — Kafka broker connection settings
// ============================================================
struct KafkaConfig {
    std::string brokers         = "localhost:9092";
    std::string security_protocol = "PLAINTEXT";  // PLAINTEXT | SASL_PLAINTEXT | SSL | SASL_SSL

    // SASL settings (if applicable)
    std::string sasl_mechanism  = "";   // PLAIN | SCRAM-SHA-256 | SCRAM-SHA-512
    std::string sasl_username   = "";
    std::string sasl_password   = "";

    // Consumer defaults
    std::string default_group_id = "kafka-mcp-server";
    int32_t     fetch_max_bytes  = 1048576;  // 1MB
    int32_t     session_timeout_ms = 10000;
};

// ============================================================
// ServerConfig — full server configuration
// ============================================================
struct ServerConfig {
    // gRPC settings
    std::string grpc_listen_address = "0.0.0.0:50051";

    // Kafka settings
    KafkaConfig kafka;
    std::string kafka_brokers;  // convenience alias for kafka.brokers

    // Logging
    std::string log_level = "info";  // trace | debug | info | warn | error
    std::string log_file  = "";      // empty = stdout only

    // Token efficiency settings
    int32_t max_messages_per_request = 10;  // Default cap for Kafka reads
    int32_t max_response_size_bytes  = 65536; // 64KB cap on response
};

// ============================================================
// ConfigLoader — loads config from JSON file
// ============================================================
class ConfigLoader {
public:
    static std::optional<ServerConfig> load(const std::string& config_file);
    static ServerConfig defaults();
};
