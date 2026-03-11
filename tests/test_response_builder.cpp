#include <gtest/gtest.h>
#include "mcp/response_builder.hpp"

// ============================================================
// ResponseBuilder Tests
// ============================================================

TEST(ResponseBuilderTest, BuildConsumeResponse_EmptyMessages) {
    ResponseBuilder builder;
    mcp::v1::ToolCallResponse resp;
    std::vector<KafkaMessageNormalized> messages;

    builder.build_consume_response(resp, messages, "test-topic");

    EXPECT_FALSE(resp.is_error());
    EXPECT_FALSE(resp.content().empty());
}

TEST(ResponseBuilderTest, BuildErrorResponse) {
    ResponseBuilder builder;
    mcp::v1::ToolCallResponse resp;

    builder.build_error_response(resp, "Kafka broker unreachable");

    EXPECT_TRUE(resp.is_error());
    EXPECT_EQ(resp.error_message(), "Kafka broker unreachable");
}

TEST(ResponseBuilderTest, TokenEstimate_ReasonableForSmallPayload) {
    ResponseBuilder builder;
    mcp::v1::ToolCallResponse resp;

    KafkaMessageNormalized msg;
    msg.topic     = "orders";
    msg.partition = 0;
    msg.offset    = 100;
    msg.key       = "order-123";
    msg.value     = R"({"order_id":"123","amount":99.99})";
    msg.timestamp_ms = 1700000000000;

    builder.build_consume_response(resp, {msg}, "orders");

    // Token estimate should be positive and reasonable
    EXPECT_GT(resp.token_count_estimate(), 0);
    EXPECT_LT(resp.token_count_estimate(), 1000); // small payload
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
