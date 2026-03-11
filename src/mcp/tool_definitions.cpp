#include "mcp/tool_definitions.hpp"

ToolDefinitions::ToolDefinitions() {
    tools_ = {
        make_consume_messages_tool(),
        make_produce_message_tool(),
        make_list_topics_tool(),
        make_describe_topic_tool(),
        make_list_consumer_groups_tool(),
        make_consumer_group_lag_tool(),
        make_cluster_metadata_tool(),
    };
}

std::vector<mcp::v1::ToolDefinition> ToolDefinitions::get_all() const {
    return tools_;
}

std::vector<mcp::v1::ToolDefinition> ToolDefinitions::get_filtered(
    const std::string& prefix) const
{
    std::vector<mcp::v1::ToolDefinition> out;
    for (const auto& t : tools_)
        if (t.name().rfind(prefix, 0) == 0)
            out.push_back(t);
    return out;
}

mcp::v1::ToolDefinition ToolDefinitions::make_consume_messages_tool() {
    mcp::v1::ToolDefinition t;
    t.set_name("consume_messages");
    t.set_description(
        "Read messages from a Kafka topic. "
        "Returns up to max_messages recent messages.");
    t.set_input_schema(
        "{\"type\":\"object\",\"properties\":{"
        "\"topic\":{\"type\":\"string\",\"description\":\"Kafka topic name\"},"
        "\"partition\":{\"type\":\"integer\",\"description\":\"Partition (-1=unassigned)\",\"default\":-1},"
        "\"start_offset\":{\"type\":\"integer\",\"description\":\"Start offset (-1=latest)\",\"default\":-1},"
        "\"max_messages\":{\"type\":\"integer\",\"description\":\"Max messages (1-100)\",\"default\":10},"
        "\"timeout_ms\":{\"type\":\"integer\",\"description\":\"Poll timeout ms\",\"default\":5000}"
        "},\"required\":[\"topic\"]}");
    return t;
}

mcp::v1::ToolDefinition ToolDefinitions::make_produce_message_tool() {
    mcp::v1::ToolDefinition t;
    t.set_name("produce_message");
    t.set_description(
        "Publish a message to a Kafka topic. "
        "Returns partition and offset of the produced message.");
    t.set_input_schema(
        "{\"type\":\"object\",\"properties\":{"
        "\"topic\":{\"type\":\"string\",\"description\":\"Kafka topic name\"},"
        "\"value\":{\"type\":\"string\",\"description\":\"Message payload\"},"
        "\"key\":{\"type\":\"string\",\"description\":\"Optional message key\"},"
        "\"partition\":{\"type\":\"integer\",\"description\":\"Target partition (-1=auto)\",\"default\":-1}"
        "},\"required\":[\"topic\",\"value\"]}");
    return t;
}

mcp::v1::ToolDefinition ToolDefinitions::make_list_topics_tool() {
    mcp::v1::ToolDefinition t;
    t.set_name("list_topics");
    t.set_description(
        "List all topics in the Kafka cluster with partition and replication info.");
    t.set_input_schema("{\"type\":\"object\",\"properties\":{}}");
    return t;
}

mcp::v1::ToolDefinition ToolDefinitions::make_describe_topic_tool() {
    mcp::v1::ToolDefinition t;
    t.set_name("describe_topic");
    t.set_description(
        "Describe a specific Kafka topic: partition count, replication factor.");
    t.set_input_schema(
        "{\"type\":\"object\",\"properties\":{"
        "\"topic\":{\"type\":\"string\",\"description\":\"Topic name to describe\"}"
        "},\"required\":[\"topic\"]}");
    return t;
}

mcp::v1::ToolDefinition ToolDefinitions::make_list_consumer_groups_tool() {
    mcp::v1::ToolDefinition t;
    t.set_name("list_consumer_groups");
    t.set_description(
        "List all consumer groups in the Kafka cluster. "
        "Requires librdkafka >= 2.0.");
    t.set_input_schema("{\"type\":\"object\",\"properties\":{}}");
    return t;
}

mcp::v1::ToolDefinition ToolDefinitions::make_consumer_group_lag_tool() {
    mcp::v1::ToolDefinition t;
    t.set_name("consumer_group_lag");
    t.set_description(
        "Get consumer group lag per partition. "
        "Lag = log end offset - committed offset.");
    t.set_input_schema(
        "{\"type\":\"object\",\"properties\":{"
        "\"group_id\":{\"type\":\"string\",\"description\":\"Consumer group ID\"},"
        "\"topic\":{\"type\":\"string\",\"description\":\"Filter by topic (optional)\"}"
        "},\"required\":[\"group_id\"]}");
    return t;
}

mcp::v1::ToolDefinition ToolDefinitions::make_cluster_metadata_tool() {
    mcp::v1::ToolDefinition t;
    t.set_name("cluster_metadata");
    t.set_description(
        "Get broker/cluster metadata: broker IDs, hosts, ports, controller.");
    t.set_input_schema("{\"type\":\"object\",\"properties\":{}}");
    return t;
}
