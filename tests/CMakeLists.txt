cmake_minimum_required(VERSION 3.16)

find_package(GTest REQUIRED)
find_package(absl REQUIRED)

set(PROTO_GEN_DIR ${CMAKE_BINARY_DIR}/generated)

set(PROTO_SRCS
    ${PROTO_GEN_DIR}/mcp_service.pb.cc
    ${PROTO_GEN_DIR}/kafka_messages.pb.cc
)
set_source_files_properties(${PROTO_SRCS} PROPERTIES GENERATED TRUE)

set(ABSL_LIBS
    absl::base
    absl::city
    absl::civil_time
    absl::cord
    absl::cord_internal
    absl::cordz_functions
    absl::cordz_handle
    absl::cordz_info
    absl::cordz_sample_token
    absl::crc32c
    absl::crc_cord_state
    absl::crc_cpu_detect
    absl::crc_internal
    absl::debugging_internal
    absl::die_if_null
    absl::examine_stack
    absl::exponential_biased
    absl::flags_commandlineflag
    absl::flags_commandlineflag_internal
    absl::flags_config
    absl::flags_internal
    absl::flags_marshalling
    absl::flags_private_handle_accessor
    absl::flags_program_name
    absl::flags_reflection
    absl::graphcycles_internal
    absl::hash
    absl::hashtablez_sampler
    absl::int128
    absl::kernel_timeout_internal
    absl::log_flags
    absl::log_globals
    absl::log_initialize
    absl::log_internal_check_op
    absl::log_internal_conditions
    absl::log_internal_fnmatch
    absl::log_internal_format
    absl::log_internal_globals
    absl::log_internal_log_sink_set
    absl::log_internal_message
    absl::log_internal_nullguard
    absl::log_internal_proto
    absl::log_severity
    absl::log_sink
    absl::low_level_hash
    absl::malloc_internal
    absl::periodic_sampler
    absl::random_distributions
    absl::random_internal_entropy_pool
    absl::random_internal_platform
    absl::random_internal_randen
    absl::random_internal_randen_hwaes
    absl::random_internal_randen_hwaes_impl
    absl::random_internal_randen_slow
    absl::random_internal_seed_material
    absl::random_seed_gen_exception
    absl::random_seed_sequences
    absl::raw_hash_set
    absl::raw_logging_internal
    absl::spinlock_wait
    absl::stacktrace
    absl::status
    absl::statusor
    absl::strerror
    absl::str_format_internal
    absl::strings
    absl::strings_internal
    absl::string_view
    absl::symbolize
    absl::synchronization
    absl::throw_delegate
    absl::time
    absl::time_zone
    absl::vlog_config_internal
)

# utf8_range is a protobuf dependency — check name with:
# ls grpcLib/lib64/ | grep utf8
set(UTF8_RANGE_LIB
    /mnt/data/AI/MCP-Project/grpc/.grpcLib/lib64/libutf8_range.a
    /mnt/data/AI/MCP-Project/grpc/.grpcLib/lib64/libutf8_validity.a
)

# ============================================================
# Phase 1 — Foundation Tests
# ============================================================
add_executable(test_phase1_foundation
    test_phase1_foundation.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/config_loader.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logger.cpp
)
target_include_directories(test_phase1_foundation PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(test_phase1_foundation PRIVATE
    GTest::GTest GTest::Main
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    /opt/rh/gcc-toolset-12/root/usr/lib/gcc/x86_64-redhat-linux/12/libstdc++fs.a
)
add_test(NAME Phase1FoundationTests COMMAND test_phase1_foundation)

# ============================================================
# Unit Tests
# ============================================================

add_executable(test_kafka_consumer
    test_kafka_consumer.cpp
)
target_include_directories(test_kafka_consumer PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${PROTO_GEN_DIR}
)
target_link_libraries(test_kafka_consumer PRIVATE
    GTest::GTest GTest::Main
    ${RDKAFKA_LIBRARIES}
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)
add_test(NAME KafkaConsumerTests COMMAND test_kafka_consumer)

add_executable(test_response_builder
    test_response_builder.cpp
    ${CMAKE_SOURCE_DIR}/src/mcp/response_builder.cpp
    ${CMAKE_SOURCE_DIR}/src/kafka/kafka_consumer.cpp
    ${CMAKE_SOURCE_DIR}/src/kafka/kafka_admin.cpp
    ${PROTO_SRCS}
)
target_include_directories(test_response_builder PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${PROTO_GEN_DIR}
    ${RDKAFKA_INCLUDE_DIRS}
)
target_link_libraries(test_response_builder PRIVATE
    GTest::GTest GTest::Main
    nlohmann_json::nlohmann_json
    protobuf::libprotobuf
    spdlog::spdlog
    ${RDKAFKA_LIBRARIES}
    ${ABSL_LIBS}
    ${UTF8_RANGE_LIB}
)
add_dependencies(test_response_builder kafka_mcp_server)
add_test(NAME ResponseBuilderTests COMMAND test_response_builder)

add_executable(test_tool_definitions
    test_tool_definitions.cpp
    ${CMAKE_SOURCE_DIR}/src/mcp/tool_definitions.cpp
    ${PROTO_SRCS}
)
target_include_directories(test_tool_definitions PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${PROTO_GEN_DIR}
)
target_link_libraries(test_tool_definitions PRIVATE
    GTest::GTest GTest::Main
    protobuf::libprotobuf
    ${ABSL_LIBS}
    ${UTF8_RANGE_LIB}
)
add_dependencies(test_tool_definitions kafka_mcp_server)
add_test(NAME ToolDefinitionTests COMMAND test_tool_definitions)