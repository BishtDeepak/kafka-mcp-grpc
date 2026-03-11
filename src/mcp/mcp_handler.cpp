#include "mcp/mcp_handler.hpp"
#include "utils/logger.hpp"

#include <nlohmann/json.hpp>
#include <stdexcept>

using json = nlohmann::json;

MCPHandler::MCPHandler(const ServerConfig& config)
    : config_(config)
    , consumer_(std::make_unique<KafkaConsumer>(config.kafka))
    , producer_(std::make_unique<KafkaProducer>(config.kafka))
    , admin_(std::make_unique<KafkaAdmin>(config.kafka))
    , tool_defs_()
    , response_builder_()
{
    register_tools();
    LOG_INFO("MCPHandler initialised with {} tools", tool_registry_.size());
}

void MCPHandler::register_tools() {
    tool_registry_["consume_messages"]     = [this](auto& req, auto& resp) {
        return handle_consume_messages(req, resp); };
    tool_registry_["produce_message"]      = [this](auto& req, auto& resp) {
        return handle_produce_message(req, resp); };
    tool_registry_["list_topics"]          = [this](auto& req, auto& resp) {
        return handle_list_topics(req, resp); };
    tool_registry_["describe_topic"]       = [this](auto& req, auto& resp) {
        return handle_describe_topic(req, resp); };
    tool_registry_["list_consumer_groups"] = [this](auto& req, auto& resp) {
        return handle_list_consumer_groups(req, resp); };
    tool_registry_["consumer_group_lag"]   = [this](auto& req, auto& resp) {
        return handle_consumer_group_lag(req, resp); };
    tool_registry_["cluster_metadata"]     = [this](auto& req, auto& resp) {
        return handle_cluster_metadata(req, resp); };
}

grpc::Status MCPHandler::ListTools(
    grpc::ServerContext* /*ctx*/,
    const mcp::v1::ListToolsRequest* request,
    mcp::v1::ListToolsResponse* response)
{
    const std::string& filter = request->filter();
    LOG_DEBUG("ListTools called, filter='{}'", filter);

    auto tools = filter.empty()
               ? tool_defs_.get_all()
               : tool_defs_.get_filtered(filter);

    for (const auto& t : tools)
        *response->add_tools() = t;

    response->set_total_count(static_cast<int32_t>(tools.size()));
    LOG_INFO("ListTools: returned {} tools", tools.size());
    return grpc::Status::OK;
}

grpc::Status MCPHandler::CallTool(
    grpc::ServerContext* /*ctx*/,
    const mcp::v1::ToolCallRequest* request,
    mcp::v1::ToolCallResponse* response)
{
    const std::string& name = request->tool_name();
    LOG_INFO("CallTool: '{}' request_id='{}'", name, request->request_id());

    response->set_request_id(request->request_id());

    auto it = tool_registry_.find(name);
    if (it == tool_registry_.end()) {
        response_builder_.build_error_response(*response, "Unknown tool: " + name);
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "Unknown tool: " + name);
    }

    try {
        return it->second(*request, *response);
    } catch (const std::exception& e) {
        LOG_ERROR("CallTool '{}' threw: {}", name, e.what());
        response_builder_.build_error_response(*response, e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

grpc::Status MCPHandler::CallToolStreaming(
    grpc::ServerContext* ctx,
    const mcp::v1::ToolCallRequest* request,
    grpc::ServerWriter<mcp::v1::ToolCallResponse>* writer)
{
    mcp::v1::ToolCallResponse resp;
    auto status = CallTool(ctx, request, &resp);
    writer->Write(resp);
    return status;
}

grpc::Status MCPHandler::HealthCheck(
    grpc::ServerContext* /*ctx*/,
    const mcp::v1::HealthCheckRequest* /*request*/,
    mcp::v1::HealthCheckResponse* response)
{
    response->set_status(mcp::v1::HealthCheckResponse::SERVING);
    response->set_version("1.0.0");
    response->set_kafka_status(
        admin_->is_connected() ? "connected" : "disconnected");
    LOG_DEBUG("HealthCheck: SERVING");
    return grpc::Status::OK;
}

// ============================================================
// Param helpers — proto uses map<string,string>, all values
// arrive as strings and must be converted as needed.
// ============================================================
using ParamMap = google::protobuf::Map<std::string, std::string>;

static std::string get_param(const ParamMap& p, const std::string& key,
                              const std::string& def = "") {
    auto it = p.find(key);
    return it != p.end() ? it->second : def;
}

static int32_t get_param_int(const ParamMap& p, const std::string& key,
                              int32_t def = 0) {
    auto it = p.find(key);
    if (it == p.end()) return def;
    try { return std::stoi(it->second); } catch (...) { return def; }
}

static int64_t get_param_int64(const ParamMap& p, const std::string& key,
                                int64_t def = 0) {
    auto it = p.find(key);
    if (it == p.end()) return def;
    try { return std::stoll(it->second); } catch (...) { return def; }
}

// ============================================================
// consume_messages
// ============================================================
grpc::Status MCPHandler::handle_consume_messages(
    const mcp::v1::ToolCallRequest& req, mcp::v1::ToolCallResponse& resp)
{
    const auto& p = req.params();

    std::string topic = get_param(p, "topic");
    if (topic.empty()) {
        response_builder_.build_error_response(resp, "Missing required parameter: topic");
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Missing topic");
    }

    int32_t partition    = get_param_int  (p, "partition",    RdKafka::Topic::PARTITION_UA);
    int64_t start_offset = get_param_int64(p, "start_offset", RdKafka::Topic::OFFSET_END);
    int32_t max_messages = get_param_int  (p, "max_messages", 10);
    int32_t timeout_ms   = get_param_int  (p, "timeout_ms",   5000);

    max_messages = std::min(max_messages, config_.max_messages_per_request);

    auto messages = consumer_->consume(topic, partition, start_offset,
                                       max_messages, timeout_ms);
    response_builder_.build_consume_response(resp, messages, topic);
    return grpc::Status::OK;
}

// ============================================================
// produce_message
// ============================================================
grpc::Status MCPHandler::handle_produce_message(
    const mcp::v1::ToolCallRequest& req, mcp::v1::ToolCallResponse& resp)
{
    const auto& p = req.params();

    std::string topic = get_param(p, "topic");
    std::string value = get_param(p, "value");

    if (topic.empty() || value.empty()) {
        response_builder_.build_error_response(resp, "Missing required parameters: topic, value");
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Missing topic or value");
    }

    std::string key   = get_param(p, "key");
    int32_t partition = get_param_int(p, "partition", RdKafka::Topic::PARTITION_UA);

    // Headers encoded as "header_<name>" entries in the params map
    std::unordered_map<std::string, std::string> headers;
    const std::string hdr_prefix = "header_";
    for (const auto& [k, v] : p)
        if (k.rfind(hdr_prefix, 0) == 0)
            headers[k.substr(hdr_prefix.size())] = v;

    auto result = producer_->produce(topic, value, key, partition, headers);

    json out;
    out["success"]   = result.success;
    out["topic"]     = result.topic;
    out["partition"] = result.partition;
    if (result.success)
        out["offset"] = result.offset;
    else
        out["error"]  = result.error_message;

    resp.set_content(out.dump());
    resp.set_is_error(!result.success);
    if (!result.success)
        resp.set_error_message(result.error_message);

    return result.success
        ? grpc::Status::OK
        : grpc::Status(grpc::StatusCode::INTERNAL, result.error_message);
}

// ============================================================
// list_topics
// ============================================================
grpc::Status MCPHandler::handle_list_topics(
    const mcp::v1::ToolCallRequest& /*req*/, mcp::v1::ToolCallResponse& resp)
{
    auto topics = admin_->list_topics();
    response_builder_.build_topics_response(resp, topics);
    return grpc::Status::OK;
}

// ============================================================
// describe_topic
// ============================================================
grpc::Status MCPHandler::handle_describe_topic(
    const mcp::v1::ToolCallRequest& req, mcp::v1::ToolCallResponse& resp)
{
    std::string topic = get_param(req.params(), "topic");
    if (topic.empty()) {
        response_builder_.build_error_response(resp, "Missing required parameter: topic");
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Missing topic");
    }

    auto tm = admin_->describe_topic(topic);
    resp.set_content(tm.to_json().dump());
    resp.set_is_error(false);
    return grpc::Status::OK;
}

// ============================================================
// list_consumer_groups
// ============================================================
grpc::Status MCPHandler::handle_list_consumer_groups(
    const mcp::v1::ToolCallRequest& /*req*/, mcp::v1::ToolCallResponse& resp)
{
    auto groups = admin_->list_consumer_groups();
    json out;
    out["groups"] = groups;
    out["count"]  = groups.size();
    resp.set_content(out.dump());
    resp.set_is_error(false);
    return grpc::Status::OK;
}

// ============================================================
// consumer_group_lag
// ============================================================
grpc::Status MCPHandler::handle_consumer_group_lag(
    const mcp::v1::ToolCallRequest& req, mcp::v1::ToolCallResponse& resp)
{
    const auto& p = req.params();
    std::string group_id = get_param(p, "group_id");
    if (group_id.empty()) {
        response_builder_.build_error_response(resp, "Missing required parameter: group_id");
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Missing group_id");
    }

    std::string topic = get_param(p, "topic");
    auto lag = admin_->get_consumer_group_lag(group_id, topic);
    response_builder_.build_lag_response(resp, lag, group_id);
    return grpc::Status::OK;
}

// ============================================================
// cluster_metadata
// ============================================================
grpc::Status MCPHandler::handle_cluster_metadata(
    const mcp::v1::ToolCallRequest& /*req*/, mcp::v1::ToolCallResponse& resp)
{
    auto brokers = admin_->get_brokers();
    response_builder_.build_brokers_response(resp, brokers);
    return grpc::Status::OK;
}
