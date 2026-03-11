cmake_minimum_required(VERSION 3.16)
project(kafka_mcp_grpc VERSION 1.0.0 LANGUAGES CXX)
# Use GCC toolset 12 libstdc++ to match custom gRPC build
# After project() line — static link GCC 12 stdlib to avoid system GCC 8 mismatch
set(GCC12_LIBDIR /opt/rh/gcc-toolset-12/root/usr/lib/gcc/x86_64-redhat-linux/12)
set(CMAKE_EXE_LINKER_FLAGS
    "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc -L${GCC12_LIBDIR}")
# ============================================================
# C++ Standard
# ============================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================
# Build Type
# ============================================================
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Project: ${PROJECT_NAME} v${PROJECT_VERSION}")

# ============================================================
# Dependencies — gRPC + Protobuf
# ============================================================
# Prefer custom gRPC/Protobuf build over system install
list(PREPEND CMAKE_PREFIX_PATH
    "/mnt/data/AI/MCP-Project/grpc/.grpcLib"
)
set(Protobuf_ROOT "/mnt/data/AI/MCP-Project/grpc/.grpcLib")

find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)

message(STATUS "Protobuf version: ${Protobuf_VERSION}")
message(STATUS "gRPC found")

# ============================================================
# Dependencies — librdkafka (Kafka C++ client)
# ============================================================
find_package(PkgConfig REQUIRED)
pkg_check_modules(RDKAFKA REQUIRED rdkafka++)

# ============================================================
# Dependencies — nlohmann/json (JSON for MCP messages)
# ============================================================
#find_package(nlohmann_json REQUIRED)
find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp
    PATHS /usr/local/include /usr/include
    REQUIRED
)
add_library(nlohmann_json::nlohmann_json INTERFACE IMPORTED)
target_include_directories(nlohmann_json::nlohmann_json
    INTERFACE ${NLOHMANN_JSON_INCLUDE_DIR}
)
# ============================================================
# Dependencies — spdlog (Logging)
# ============================================================
find_package(spdlog REQUIRED)

# ============================================================
# Protobuf Code Generation
# ============================================================
set(PROTO_FILES
    proto/mcp_service.proto
    proto/kafka_messages.proto
)

# Generate .pb.cc and .grpc.pb.cc from .proto files
set(PROTO_SRC_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${PROTO_SRC_DIR})

foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)

    set(PROTO_SRC  "${PROTO_SRC_DIR}/${PROTO_NAME}.pb.cc")
    set(PROTO_HDR  "${PROTO_SRC_DIR}/${PROTO_NAME}.pb.h")
    set(GRPC_SRC   "${PROTO_SRC_DIR}/${PROTO_NAME}.grpc.pb.cc")
    set(GRPC_HDR   "${PROTO_SRC_DIR}/${PROTO_NAME}.grpc.pb.h")

    add_custom_command(
        OUTPUT  ${PROTO_SRC} ${PROTO_HDR} ${GRPC_SRC} ${GRPC_HDR}
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        ARGS    --grpc_out=${PROTO_SRC_DIR}
                --cpp_out=${PROTO_SRC_DIR}
                --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
                -I${CMAKE_CURRENT_SOURCE_DIR}/proto
                ${CMAKE_CURRENT_SOURCE_DIR}/${PROTO_FILE}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${PROTO_FILE}
        COMMENT "Generating Protobuf/gRPC code from ${PROTO_FILE}"
    )

    list(APPEND PROTO_SRCS ${PROTO_SRC} ${GRPC_SRC})
    list(APPEND PROTO_HDRS ${PROTO_HDR} ${GRPC_HDR})
endforeach()

# ============================================================
# Source Files
# ============================================================
set(SERVER_SRCS
    src/server/mcp_grpc_server.cpp
    src/server/tool_registry.cpp
)

set(KAFKA_SRCS
    src/kafka/kafka_consumer.cpp
    src/kafka/kafka_producer.cpp
    src/kafka/kafka_admin.cpp
)

set(MCP_SRCS
    src/mcp/mcp_handler.cpp
    src/mcp/tool_definitions.cpp
    src/mcp/response_builder.cpp
)

set(UTILS_SRCS
    src/utils/logger.cpp
    src/utils/config_loader.cpp
    src/utils/json_utils.cpp
)

# ============================================================
# Main Executable
# ============================================================
add_executable(kafka_mcp_server
    src/main.cpp
    ${SERVER_SRCS}
    ${KAFKA_SRCS}
    ${MCP_SRCS}
    ${UTILS_SRCS}
    ${PROTO_SRCS}
)

target_include_directories(kafka_mcp_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROTO_SRC_DIR}
    ${RDKAFKA_INCLUDE_DIRS}
)

target_link_libraries(kafka_mcp_server PRIVATE
    gRPC::grpc++
    gRPC::grpc++_reflection
    protobuf::libprotobuf
    ${RDKAFKA_LIBRARIES}
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    pthread
)

target_compile_options(kafka_mcp_server PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3>
)

# ============================================================
# Tests (Google Test)
# ============================================================
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)
    add_subdirectory(tests)
endif()

# ============================================================
# Install
# ============================================================
install(TARGETS kafka_mcp_server DESTINATION bin)
