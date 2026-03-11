#include <gtest/gtest.h>
#include "mcp/tool_definitions.hpp"

// ============================================================
// ToolDefinitions Unit Tests
// ============================================================

class ToolDefinitionsTest : public ::testing::Test {
protected:
    ToolDefinitions defs;
};

TEST_F(ToolDefinitionsTest, GetAllReturnsSevenTools) {
    auto tools = defs.get_all();
    EXPECT_EQ(tools.size(), 7u);
}

TEST_F(ToolDefinitionsTest, AllToolsHaveNonEmptyName) {
    for (const auto& t : defs.get_all())
        EXPECT_FALSE(t.name().empty()) << "Tool has empty name";
}

TEST_F(ToolDefinitionsTest, AllToolsHaveNonEmptyDescription) {
    for (const auto& t : defs.get_all())
        EXPECT_FALSE(t.description().empty())
            << "Tool '" << t.name() << "' has empty description";
}

TEST_F(ToolDefinitionsTest, AllToolsHaveInputSchema) {
    for (const auto& t : defs.get_all())
        EXPECT_FALSE(t.input_schema().empty())
            << "Tool '" << t.name() << "' has empty input_schema";
}

TEST_F(ToolDefinitionsTest, ConsumeMessagesToolExists) {
    auto tools = defs.get_all();
    auto it = std::find_if(tools.begin(), tools.end(),
        [](const auto& t){ return t.name() == "consume_messages"; });
    EXPECT_NE(it, tools.end());
}

TEST_F(ToolDefinitionsTest, ProduceMessageToolExists) {
    auto tools = defs.get_all();
    auto it = std::find_if(tools.begin(), tools.end(),
        [](const auto& t){ return t.name() == "produce_message"; });
    EXPECT_NE(it, tools.end());
}

TEST_F(ToolDefinitionsTest, FilterByPrefixReturnsSubset) {
    auto filtered = defs.get_filtered("consume");
    EXPECT_EQ(filtered.size(), 1u);
    EXPECT_EQ(filtered[0].name(), "consume_messages");
}

TEST_F(ToolDefinitionsTest, FilterByEmptyPrefixIsSubsetOfAll) {
    // get_filtered("") should return all tools starting with ""
    auto filtered = defs.get_filtered("");
    auto all      = defs.get_all();
    EXPECT_EQ(filtered.size(), all.size());
}

TEST_F(ToolDefinitionsTest, FilterByUnknownPrefixReturnsEmpty) {
    auto filtered = defs.get_filtered("zzz_nonexistent");
    EXPECT_TRUE(filtered.empty());
}

TEST_F(ToolDefinitionsTest, ClusterMetadataToolExists) {
    auto tools = defs.get_all();
    auto it = std::find_if(tools.begin(), tools.end(),
        [](const auto& t){ return t.name() == "cluster_metadata"; });
    EXPECT_NE(it, tools.end());
}
