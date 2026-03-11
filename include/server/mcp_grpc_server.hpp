#pragma once

#include <string>
#include <memory>
#include <thread>

#include <grpcpp/grpcpp.h>

#include "mcp/mcp_handler.hpp"
#include "utils/config_loader.hpp"

// ============================================================
// MCPGrpcServer
// Starts and manages the gRPC server exposing MCP tools
// ============================================================
class MCPGrpcServer {
public:
    explicit MCPGrpcServer(const ServerConfig& config);
    ~MCPGrpcServer();

    // Non-copyable
    MCPGrpcServer(const MCPGrpcServer&) = delete;
    MCPGrpcServer& operator=(const MCPGrpcServer&) = delete;

    bool start();
    void stop();
    bool is_running() const;

private:
    ServerConfig                        config_;
    std::unique_ptr<grpc::Server>       grpc_server_;
    std::unique_ptr<MCPHandler>         mcp_handler_;
    std::atomic<bool>                   running_{false};
};
