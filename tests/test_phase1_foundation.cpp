#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>

#include "utils/config_loader.hpp"
#include "utils/logger.hpp"

namespace fs = std::filesystem;

// ============================================================
// Test Fixtures — create/cleanup temp config files
// ============================================================
class ConfigLoaderTest : public ::testing::Test {
protected:
    std::string tmp_config = "/tmp/test_server.json";

    void write_config(const std::string& content) {
        std::ofstream f(tmp_config);
        f << content;
    }

    void TearDown() override {
        fs::remove(tmp_config);
    }
};

// ============================================================
// ConfigLoader Tests
// ============================================================

TEST_F(ConfigLoaderTest, LoadsValidConfig) {
    write_config(R"({
        "grpc_listen_address": "0.0.0.0:50051",
        "kafka": {
            "brokers": "localhost:9092",
            "security_protocol": "PLAINTEXT",
            "default_group_id": "test-group",
            "fetch_max_bytes": 1048576,
            "session_timeout_ms": 10000
        },
        "log_level": "debug",
        "log_file": "",
        "token_efficiency": {
            "max_messages_per_request": 10,
            "max_response_size_bytes": 65536
        }
    })");

    auto cfg = ConfigLoader::load(tmp_config);

    ASSERT_TRUE(cfg.has_value());
    EXPECT_EQ(cfg->grpc_listen_address, "0.0.0.0:50051");
    EXPECT_EQ(cfg->kafka.brokers, "localhost:9092");
    EXPECT_EQ(cfg->kafka.security_protocol, "PLAINTEXT");
    EXPECT_EQ(cfg->kafka.default_group_id, "test-group");
    EXPECT_EQ(cfg->log_level, "debug");
    EXPECT_EQ(cfg->max_messages_per_request, 10);
    EXPECT_EQ(cfg->kafka_brokers, "localhost:9092"); // alias in sync
}

TEST_F(ConfigLoaderTest, ReturnNulloptForMissingFile) {
    auto cfg = ConfigLoader::load("/tmp/does_not_exist.json");
    EXPECT_FALSE(cfg.has_value());
}

TEST_F(ConfigLoaderTest, ReturnNulloptForInvalidJSON) {
    write_config("{ this is not valid json }");
    auto cfg = ConfigLoader::load(tmp_config);
    EXPECT_FALSE(cfg.has_value());
}

TEST_F(ConfigLoaderTest, FallsBackToDefaultsForMissingFields) {
    // Minimal config — only brokers, rest should use defaults
    write_config(R"({
        "kafka": { "brokers": "mybroker:9092" }
    })");

    auto cfg = ConfigLoader::load(tmp_config);
    ASSERT_TRUE(cfg.has_value());

    // Provided value
    EXPECT_EQ(cfg->kafka.brokers, "mybroker:9092");

    // Defaults should be intact
    EXPECT_EQ(cfg->grpc_listen_address, "0.0.0.0:50051");
    EXPECT_EQ(cfg->log_level, "info");
    EXPECT_EQ(cfg->max_messages_per_request, 10);
}

TEST_F(ConfigLoaderTest, RejectsEmptyBrokers) {
    write_config(R"({
        "kafka": { "brokers": "" }
    })");

    auto cfg = ConfigLoader::load(tmp_config);
    EXPECT_FALSE(cfg.has_value());
}

TEST_F(ConfigLoaderTest, DefaultsAreValid) {
    auto cfg = ConfigLoader::defaults();
    EXPECT_FALSE(cfg.kafka.brokers.empty());
    EXPECT_FALSE(cfg.grpc_listen_address.empty());
    EXPECT_GT(cfg.max_messages_per_request, 0);
    EXPECT_GT(cfg.max_response_size_bytes, 0);
    EXPECT_EQ(cfg.kafka_brokers, cfg.kafka.brokers); // alias in sync
}

// ============================================================
// Logger Tests
// ============================================================

TEST(LoggerTest, InitWithInfoLevel) {
    // Should not throw
    EXPECT_NO_THROW(Logger::init("info", ""));
}

TEST(LoggerTest, InitWithDebugLevel) {
    EXPECT_NO_THROW(Logger::init("debug", ""));
}

TEST(LoggerTest, InitWithUnknownLevelDefaultsToInfo) {
    EXPECT_NO_THROW(Logger::init("banana", ""));
}

TEST(LoggerTest, InitWithFileOutput) {
    std::string log_path = "/tmp/test_kafka_mcp.log";
    EXPECT_NO_THROW(Logger::init("info", log_path));

    // Write a log entry and verify file was created
    LOG_INFO("Test log entry from unit test");
    spdlog::default_logger()->flush();

    EXPECT_TRUE(fs::exists(log_path));
    fs::remove(log_path);
}

TEST(LoggerTest, MacrosWorkAfterInit) {
    Logger::init("trace", "");
    // These should not crash
    EXPECT_NO_THROW({
        LOG_TRACE("trace message");
        LOG_DEBUG("debug message");
        LOG_INFO("info message");
        LOG_WARN("warn message");
        LOG_ERROR("error message");
    });
}

// ============================================================
// Main
// ============================================================
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
