#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <functional>

#include <grpcpp/grpcpp.h>

// Generated from proto
#include "mcp_service.grpc.pb.h"

#include "kafka/kafka_consumer.hpp"
#include "kafka/kafka_producer.hpp"
#include "kafka/kafka_admin.hpp"
#include "mcp/tool_definitions.hpp"
#include "mcp/response_builder.hpp"
#include "utils/config_loader.hpp"

// ============================================================
// MCPHandler
// Implements the gRPC MCPService — the core of our MCP server.
// Routes tool calls to the appropriate Kafka operation.
// ============================================================
class MCPHandler final : public mcp::v1::MCPService::Service {
public:
    explicit MCPHandler(const ServerConfig& config);
    ~MCPHandler() = default;

    // --- gRPC service methods (override from generated base) ---

    grpc::Status ListTools(
        grpc::ServerContext* ctx,
        const mcp::v1::ListToolsRequest* request,
        mcp::v1::ListToolsResponse* response) override;

    grpc::Status CallTool(
        grpc::ServerContext* ctx,
        const mcp::v1::ToolCallRequest* request,
        mcp::v1::ToolCallResponse* response) override;

    grpc::Status CallToolStreaming(
        grpc::ServerContext* ctx,
        const mcp::v1::ToolCallRequest* request,
        grpc::ServerWriter<mcp::v1::ToolCallResponse>* writer) override;

    grpc::Status HealthCheck(
        grpc::ServerContext* ctx,
        const mcp::v1::HealthCheckRequest* request,
        mcp::v1::HealthCheckResponse* response) override;

private:
    // --- Tool dispatcher ---
    using ToolHandler = std::function<
        grpc::Status(const mcp::v1::ToolCallRequest&, mcp::v1::ToolCallResponse&)
    >;

    void register_tools();

    // --- Individual tool handlers ---
    grpc::Status handle_consume_messages(
        const mcp::v1::ToolCallRequest& req, mcp::v1::ToolCallResponse& resp);

    grpc::Status handle_produce_message(
        const mcp::v1::ToolCallRequest& req, mcp::v1::ToolCallResponse& resp);

    grpc::Status handle_list_topics(
        const mcp::v1::ToolCallRequest& req, mcp::v1::ToolCallResponse& resp);

    grpc::Status handle_describe_topic(
        const mcp::v1::ToolCallRequest& req, mcp::v1::ToolCallResponse& resp);

    grpc::Status handle_list_consumer_groups(
        const mcp::v1::ToolCallRequest& req, mcp::v1::ToolCallResponse& resp);

    grpc::Status handle_consumer_group_lag(
        const mcp::v1::ToolCallRequest& req, mcp::v1::ToolCallResponse& resp);

    grpc::Status handle_cluster_metadata(
        const mcp::v1::ToolCallRequest& req, mcp::v1::ToolCallResponse& resp);

    // --- Members ---
    ServerConfig                                    config_;
    std::unique_ptr<KafkaConsumer>                  consumer_;
    std::unique_ptr<KafkaProducer>                  producer_;
    std::unique_ptr<KafkaAdmin>                     admin_;
    std::unordered_map<std::string, ToolHandler>    tool_registry_;
    ToolDefinitions                                 tool_defs_;
    ResponseBuilder                                 response_builder_;
};
