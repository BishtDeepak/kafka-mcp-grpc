#pragma once

#include <string>
#include <vector>

#include "mcp_service.pb.h"

// ============================================================
// ToolDefinitions
// Defines all MCP tools exposed to the LLM.
// Each tool has: name, description, JSON Schema for inputs.
// This is what Claude sees when it calls ListTools.
// ============================================================
class ToolDefinitions {
public:
    ToolDefinitions();

    // Returns all tool definitions
    std::vector<mcp::v1::ToolDefinition> get_all() const;

    // Returns definitions filtered by prefix
    std::vector<mcp::v1::ToolDefinition> get_filtered(
        const std::string& prefix) const;

private:
    std::vector<mcp::v1::ToolDefinition> tools_;

    // Tool builders
    mcp::v1::ToolDefinition make_consume_messages_tool();
    mcp::v1::ToolDefinition make_produce_message_tool();
    mcp::v1::ToolDefinition make_list_topics_tool();
    mcp::v1::ToolDefinition make_describe_topic_tool();
    mcp::v1::ToolDefinition make_list_consumer_groups_tool();
    mcp::v1::ToolDefinition make_consumer_group_lag_tool();
    mcp::v1::ToolDefinition make_cluster_metadata_tool();
};
