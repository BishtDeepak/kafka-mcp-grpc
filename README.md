# Kafka MCP gRPC Server (C++)

A high-performance **MCP (Model Context Protocol) server** written in C++ that exposes Apache Kafka operations as MCP tools over **gRPC + Protobuf**. This allows AI agents like Claude to interact with Kafka clusters using natural language.

---

## Architecture

```
Claude / AI Agent
      │
      │  gRPC + Protobuf (binary, fast)
      ▼
┌─────────────────────┐
│   MCPGrpcServer     │  ← gRPC server (port 50051)
│   MCPHandler        │  ← Routes tool calls
├─────────────────────┤
│   KafkaConsumer     │  ← Read messages
│   KafkaProducer     │  ← Write messages
│   KafkaAdmin        │  ← Metadata, lag, brokers
└─────────────────────┘
      │
      │  librdkafka
      ▼
 Kafka Broker(s)
```

---

## MCP Tools Exposed

| Tool | Description |
|---|---|
| `kafka_consume_messages` | Read messages from a topic |
| `kafka_produce_message` | Write a message to a topic |
| `kafka_list_topics` | List all topics in the cluster |
| `kafka_describe_topic` | Get metadata for a specific topic |
| `kafka_list_consumer_groups` | List all consumer groups |
| `kafka_consumer_group_lag` | Get lag for a consumer group |
| `kafka_cluster_metadata` | Get broker and cluster info |

---

## Project Structure

```
kafka-mcp-grpc/
├── proto/
│   ├── mcp_service.proto       # MCP gRPC service definition
│   └── kafka_messages.proto    # Kafka domain types
├── include/
│   ├── server/
│   │   └── mcp_grpc_server.hpp
│   ├── kafka/
│   │   ├── kafka_consumer.hpp
│   │   ├── kafka_producer.hpp
│   │   └── kafka_admin.hpp
│   ├── mcp/
│   │   ├── mcp_handler.hpp
│   │   ├── tool_definitions.hpp
│   │   └── response_builder.hpp
│   └── utils/
│       ├── logger.hpp
│       └── config_loader.hpp
├── src/
│   ├── main.cpp
│   ├── server/
│   ├── kafka/
│   ├── mcp/
│   └── utils/
├── tests/
├── config/
│   └── server.json
├── scripts/
│   └── build.sh
└── CMakeLists.txt
```

---

## Dependencies

| Library | Purpose |
|---|---|
| gRPC | Transport layer |
| Protobuf | Message serialization |
| librdkafka | Kafka C++ client |
| nlohmann/json | JSON for MCP responses |
| spdlog | Logging |
| GoogleTest | Unit testing |

### Install on Ubuntu/Debian
```bash
sudo apt install -y \
    libgrpc++-dev \
    libprotobuf-dev \
    protobuf-compiler-grpc \
    librdkafka-dev \
    nlohmann-json3-dev \
    libspdlog-dev \
    libgtest-dev
```

---

## Build

```bash
chmod +x scripts/build.sh
./scripts/build.sh Release
```

---

## Run

```bash
./build/kafka_mcp_server config/server.json
```

---

## Configuration

Edit `config/server.json`:

```json
{
    "grpc_listen_address": "0.0.0.0:50051",
    "kafka": {
        "brokers": "localhost:9092",
        "security_protocol": "PLAINTEXT"
    },
    "log_level": "info"
}
```

---

## Token Efficiency

This server is designed with **token efficiency** in mind:
- Responses are compact JSON (no whitespace)
- Message reads are capped at `max_messages_per_request` (default: 10)
- Response size is capped at `max_response_size_bytes` (default: 64KB)
- Unnecessary fields are stripped before returning to the LLM

---

*Built with C++ for maximum performance in high-throughput Kafka environments.*
