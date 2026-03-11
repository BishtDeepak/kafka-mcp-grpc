#include "server/mcp_grpc_server.hpp"
#include "utils/logger.hpp"

#include <grpcpp/server_builder.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>

// ============================================================
// MCPGrpcServer
// ============================================================

MCPGrpcServer::MCPGrpcServer(const ServerConfig& config)
    : config_(config)
    , mcp_handler_(std::make_unique<MCPHandler>(config))
{}

MCPGrpcServer::~MCPGrpcServer() {
    stop();
}

bool MCPGrpcServer::start() {
    if (running_) {
        LOG_WARN("MCPGrpcServer::start called but server is already running");
        return true;
    }

    grpc::ServerBuilder builder;

    // Bind address (insecure — add TLS credentials here for production)
    builder.AddListeningPort(config_.grpc_listen_address,
                             grpc::InsecureServerCredentials());

    // Register our MCP service implementation
    builder.RegisterService(mcp_handler_.get());

    // Tune channel arguments for long-lived connections
    builder.AddChannelArgument(GRPC_ARG_KEEPALIVE_TIME_MS,      30000);
    builder.AddChannelArgument(GRPC_ARG_KEEPALIVE_TIMEOUT_MS,   10000);
    builder.AddChannelArgument(GRPC_ARG_MAX_RECEIVE_MESSAGE_LENGTH, 64 * 1024 * 1024);
    builder.AddChannelArgument(GRPC_ARG_MAX_SEND_MESSAGE_LENGTH,    64 * 1024 * 1024);

    // Enable gRPC reflection — allows grpcurl and other tools to
    // discover services without needing the .proto file
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();

    grpc_server_ = builder.BuildAndStart();
    if (!grpc_server_) {
        LOG_ERROR("Failed to start gRPC server on {}", config_.grpc_listen_address);
        return false;
    }

    running_ = true;
    LOG_INFO("MCPGrpcServer listening on {}", config_.grpc_listen_address);
    return true;
}

void MCPGrpcServer::stop() {
    if (!running_) return;

    LOG_INFO("MCPGrpcServer shutting down...");
    if (grpc_server_) {
        grpc_server_->Shutdown();
        grpc_server_->Wait();
        grpc_server_ = nullptr;
    }
    running_ = false;
    LOG_INFO("MCPGrpcServer stopped");
}

bool MCPGrpcServer::is_running() const {
    return running_;
}