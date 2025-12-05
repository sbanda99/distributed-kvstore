.PHONY: all clean help build-dirs proto

# Configuration
CONDA_ENV = mapreduce
BUILD_DIR = build
PROTO_DIR = proto
SRC_DIR = src

# Conda environment (hardcoded path - adjust accordingly)
CONDA_PREFIX = /scratch/sqb6440/miniconda3/envs/mapreduce
PKG_CONFIG_PATH = $(CONDA_PREFIX)/lib/pkgconfig
PATH := $(CONDA_PREFIX)/bin:$(PATH)

# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g
CPPFLAGS = -I$(CONDA_PREFIX)/include -I$(SRC_DIR) -I$(BUILD_DIR)/$(PROTO_DIR)

# Default LDFLAGS (for local builds with conda)
LDFLAGS = -L$(CONDA_PREFIX)/lib -Wl,-rpath,$(CONDA_PREFIX)/lib -lprotobuf -lgrpc++ -lgrpc++_reflection -lgpr -labsl_synchronization -labsl_strings -labsl_base -labsl_raw_logging_internal -ldl -lpthread

# Proto files
PROTO_FILE = $(PROTO_DIR)/kvstore.proto
PROTO_NAME = kvstore
PROTO_BUILD_DIR = $(BUILD_DIR)/$(PROTO_DIR)
PROTO_PB_CC = $(PROTO_BUILD_DIR)/$(PROTO_NAME).pb.cc
PROTO_PB_H = $(PROTO_BUILD_DIR)/$(PROTO_NAME).pb.h
PROTO_GRPC_CC = $(PROTO_BUILD_DIR)/$(PROTO_NAME).grpc.pb.cc
PROTO_GRPC_H = $(PROTO_BUILD_DIR)/$(PROTO_NAME).grpc.pb.h
PROTO_PB_OBJ = $(PROTO_BUILD_DIR)/$(PROTO_NAME).pb.o
PROTO_GRPC_OBJ = $(PROTO_BUILD_DIR)/$(PROTO_NAME).grpc.pb.o
PROTO_OBJS = $(PROTO_PB_OBJ) $(PROTO_GRPC_OBJ)

# Common object files
COMMON_SRCS = $(SRC_DIR)/common/config.cpp $(SRC_DIR)/common/utils.cpp
COMMON_OBJS = $(COMMON_SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# Protocol object files
PROTOCOL_SRCS = $(SRC_DIR)/protocol/abd.cpp $(SRC_DIR)/protocol/blocking.cpp
PROTOCOL_OBJS = $(PROTOCOL_SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# Client object files
CLIENT_SRCS = $(SRC_DIR)/client/abd_client.cpp $(SRC_DIR)/client/abd_client_impl.cpp \
              $(SRC_DIR)/client/blocking_client.cpp $(SRC_DIR)/client/blocking_client_impl.cpp
CLIENT_OBJS = $(CLIENT_SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# Server executables
ABD_SERVER_SRC = $(SRC_DIR)/server/abd_server.cpp
ABD_SERVER_OBJ = $(BUILD_DIR)/server/abd_server.o
ABD_SERVER = $(BUILD_DIR)/abd_server

BLOCKING_SERVER_SRC = $(SRC_DIR)/server/blocking_server.cpp
BLOCKING_SERVER_OBJ = $(BUILD_DIR)/server/blocking_server.o
BLOCKING_SERVER = $(BUILD_DIR)/blocking_server

# Client executables
ABD_CLIENT_SRC = $(SRC_DIR)/client/abd_client_main.cpp
ABD_CLIENT_OBJ = $(BUILD_DIR)/client/abd_client_main.o
ABD_CLIENT = $(BUILD_DIR)/abd_client

BLOCKING_CLIENT_SRC = $(SRC_DIR)/client/blocking_client_main.cpp
BLOCKING_CLIENT_OBJ = $(BUILD_DIR)/client/blocking_client_main.o
BLOCKING_CLIENT = $(BUILD_DIR)/blocking_client

# Test executables
TEST_ABD_SRC = tests/test_correctness_abd.cpp
TEST_ABD_OBJ = $(BUILD_DIR)/tests/test_correctness_abd.o
TEST_ABD = $(BUILD_DIR)/test_correctness_abd

TEST_BLOCKING_SRC = tests/test_correctness_blocking.cpp
TEST_BLOCKING_OBJ = $(BUILD_DIR)/tests/test_correctness_blocking.o
TEST_BLOCKING = $(BUILD_DIR)/test_correctness_blocking

# Evaluation executables
EVAL_PERF_SRC = evaluation/evaluate_performance.cpp
EVAL_PERF_OBJ = $(BUILD_DIR)/evaluation/evaluate_performance.o
EVAL_PERF = $(BUILD_DIR)/evaluate_performance

EVAL_CRASH_SRC = evaluation/evaluate_crash_impact.cpp
EVAL_CRASH_OBJ = $(BUILD_DIR)/evaluation/evaluate_crash_impact.o
EVAL_CRASH = $(BUILD_DIR)/evaluate_crash_impact

# Default target
all: build-dirs proto $(ABD_SERVER) $(BLOCKING_SERVER) $(ABD_CLIENT) $(BLOCKING_CLIENT) \
     $(TEST_ABD) $(TEST_BLOCKING) $(EVAL_PERF) $(EVAL_CRASH)
	@echo ""
	@echo "Build complete! Binaries are in $(BUILD_DIR)/"
	@echo "Servers:"
	@echo "  $(ABD_SERVER)"
	@echo "  $(BLOCKING_SERVER)"
	@echo "Clients:"
	@echo "  $(ABD_CLIENT)"
	@echo "  $(BLOCKING_CLIENT)"
	@echo "Tests:"
	@echo "  $(TEST_ABD)"
	@echo "  $(TEST_BLOCKING)"
	@echo "Evaluation:"
	@echo "  $(EVAL_PERF)"
	@echo "  $(EVAL_CRASH)"
	@echo ""

# Bundle required shared libraries
# This finds all dependencies of the binaries and copies them
bundle-libs:
	@echo "Bundling shared libraries..."
	@mkdir -p $(BUILD_DIR)/lib
	@echo "Finding dependencies of binaries..."
	@for bin in $(BUILD_DIR)/abd_server $(BUILD_DIR)/blocking_server \
	            $(BUILD_DIR)/abd_client $(BUILD_DIR)/blocking_client \
	            $(BUILD_DIR)/test_correctness_abd $(BUILD_DIR)/test_correctness_blocking \
	            $(BUILD_DIR)/evaluate_performance $(BUILD_DIR)/evaluate_crash_impact; do \
		if [ -f $$bin ] && [ -x $$bin ]; then \
			echo "  Analyzing $$(basename $$bin)..."; \
			ldd $$bin 2>/dev/null | grep "$(CONDA_PREFIX)/lib" | awk '{print $$3}' | while read lib; do \
				if [ -n "$$lib" ] && [ -f "$$lib" ]; then \
					libname=$$(basename $$lib); \
					if [ ! -f "$(BUILD_DIR)/lib/$$libname" ]; then \
						cp -L $$lib $(BUILD_DIR)/lib/ 2>/dev/null && echo "    Copied $$libname" || true; \
					fi; \
				fi; \
			done; \
		fi; \
	done
	@echo "Copied libraries to $(BUILD_DIR)/lib/"
	@libcount=$$(ls -1 $(BUILD_DIR)/lib/*.so* 2>/dev/null | wc -l); \
	echo "  Total libraries bundled: $$libcount"

# Create build directories
build-dirs:
	@mkdir -p $(BUILD_DIR)/common
	@mkdir -p $(BUILD_DIR)/protocol
	@mkdir -p $(BUILD_DIR)/client
	@mkdir -p $(BUILD_DIR)/server
	@mkdir -p $(BUILD_DIR)/tests
	@mkdir -p $(BUILD_DIR)/evaluation
	@mkdir -p $(PROTO_BUILD_DIR)

# Generate proto files
proto: build-dirs $(PROTO_PB_CC) $(PROTO_GRPC_CC) $(PROTO_OBJS)

GRPC_CPP_PLUGIN = grpc_cpp_plugin
GRPC_CPP_PLUGIN_PATH = $(shell which $(GRPC_CPP_PLUGIN))

$(PROTO_PB_CC) $(PROTO_GRPC_CC): $(PROTO_FILE)
	@echo "Generating proto files..."
	@echo "Using grpc_cpp_plugin: $(GRPC_CPP_PLUGIN_PATH)"
	@protoc --cpp_out=$(PROTO_BUILD_DIR) \
	       --grpc_out=$(PROTO_BUILD_DIR) \
	       --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) \
	       -I$(PROTO_DIR) \
	       $(PROTO_FILE)

# Compile proto-generated files
$(PROTO_PB_OBJ): $(PROTO_PB_CC)
	@echo "Compiling $<..."
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(PROTO_GRPC_OBJ): $(PROTO_GRPC_CC)
	@echo "Compiling $<..."
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Common object files
$(BUILD_DIR)/common/%.o: $(SRC_DIR)/common/%.cpp
	@echo "Compiling $<..."
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Protocol object files
$(BUILD_DIR)/protocol/%.o: $(SRC_DIR)/protocol/%.cpp
	@echo "Compiling $<..."
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Client object files
$(BUILD_DIR)/client/%.o: $(SRC_DIR)/client/%.cpp $(PROTO_GRPC_H)
	@echo "Compiling $<..."
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# ABD Server
$(ABD_SERVER): $(ABD_SERVER_OBJ) $(PROTO_OBJS) $(COMMON_OBJS) $(PROTOCOL_OBJS)
	@echo "Linking $(ABD_SERVER)..."
	@$(CXX) $(CXXFLAGS) -o $@ $(ABD_SERVER_OBJ) $(PROTO_OBJS) $(COMMON_OBJS) $(PROTOCOL_OBJS) \
		$(LDFLAGS)

$(ABD_SERVER_OBJ): $(ABD_SERVER_SRC) $(PROTO_GRPC_H)
	@echo "Compiling $<..."
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Blocking Server
$(BLOCKING_SERVER): $(BLOCKING_SERVER_OBJ) $(PROTO_OBJS) $(COMMON_OBJS) $(PROTOCOL_OBJS)
	@echo "Linking $(BLOCKING_SERVER)..."
	@$(CXX) $(CXXFLAGS) -o $@ $(BLOCKING_SERVER_OBJ) $(PROTO_OBJS) $(COMMON_OBJS) $(PROTOCOL_OBJS) \
		$(LDFLAGS)

$(BLOCKING_SERVER_OBJ): $(BLOCKING_SERVER_SRC) $(PROTO_GRPC_H)
	@echo "Compiling $<..."
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# ABD Client
$(ABD_CLIENT): $(ABD_CLIENT_OBJ) $(PROTO_OBJS) $(CLIENT_OBJS) $(COMMON_OBJS) $(PROTOCOL_OBJS)
	@echo "Linking $(ABD_CLIENT)..."
	@$(CXX) $(CXXFLAGS) -o $@ $(ABD_CLIENT_OBJ) $(PROTO_OBJS) $(CLIENT_OBJS) $(COMMON_OBJS) $(PROTOCOL_OBJS) \
		$(LDFLAGS)

$(ABD_CLIENT_OBJ): $(ABD_CLIENT_SRC) $(PROTO_GRPC_H)
	@echo "Compiling $<..."
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Blocking Client
$(BLOCKING_CLIENT): $(BLOCKING_CLIENT_OBJ) $(PROTO_OBJS) $(CLIENT_OBJS) $(COMMON_OBJS) $(PROTOCOL_OBJS)
	@echo "Linking $(BLOCKING_CLIENT)..."
	@$(CXX) $(CXXFLAGS) -o $@ $(BLOCKING_CLIENT_OBJ) $(PROTO_OBJS) $(CLIENT_OBJS) $(COMMON_OBJS) $(PROTOCOL_OBJS) \
		$(LDFLAGS)

$(BLOCKING_CLIENT_OBJ): $(BLOCKING_CLIENT_SRC) $(PROTO_GRPC_H)
	@echo "Compiling $<..."
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Test executables
$(TEST_ABD): $(TEST_ABD_OBJ) $(PROTO_OBJS) $(CLIENT_OBJS) $(COMMON_OBJS) $(PROTOCOL_OBJS)
	@echo "Linking $(TEST_ABD)..."
	@$(CXX) $(CXXFLAGS) -o $@ $(TEST_ABD_OBJ) $(PROTO_OBJS) $(CLIENT_OBJS) $(COMMON_OBJS) $(PROTOCOL_OBJS) \
		$(LDFLAGS)

$(TEST_ABD_OBJ): $(TEST_ABD_SRC) $(PROTO_GRPC_H)
	@echo "Compiling $<..."
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(TEST_BLOCKING): $(TEST_BLOCKING_OBJ) $(PROTO_OBJS) $(CLIENT_OBJS) $(COMMON_OBJS) $(PROTOCOL_OBJS)
	@echo "Linking $(TEST_BLOCKING)..."
	@$(CXX) $(CXXFLAGS) -o $@ $(TEST_BLOCKING_OBJ) $(PROTO_OBJS) $(CLIENT_OBJS) $(COMMON_OBJS) $(PROTOCOL_OBJS) \
		$(LDFLAGS)

$(TEST_BLOCKING_OBJ): $(TEST_BLOCKING_SRC) $(PROTO_GRPC_H)
	@echo "Compiling $<..."
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Evaluation executables
$(EVAL_PERF): $(EVAL_PERF_OBJ) $(PROTO_OBJS) $(CLIENT_OBJS) $(COMMON_OBJS) $(PROTOCOL_OBJS)
	@echo "Linking $(EVAL_PERF)..."
	@$(CXX) $(CXXFLAGS) -o $@ $(EVAL_PERF_OBJ) $(PROTO_OBJS) $(CLIENT_OBJS) $(COMMON_OBJS) $(PROTOCOL_OBJS) \
		$(LDFLAGS)

$(EVAL_PERF_OBJ): $(EVAL_PERF_SRC) $(PROTO_GRPC_H)
	@echo "Compiling $<..."
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(EVAL_CRASH): $(EVAL_CRASH_OBJ) $(PROTO_OBJS) $(CLIENT_OBJS) $(COMMON_OBJS) $(PROTOCOL_OBJS)
	@echo "Linking $(EVAL_CRASH)..."
	@$(CXX) $(CXXFLAGS) -o $@ $(EVAL_CRASH_OBJ) $(PROTO_OBJS) $(CLIENT_OBJS) $(COMMON_OBJS) $(PROTOCOL_OBJS) \
		$(LDFLAGS)

$(EVAL_CRASH_OBJ): $(EVAL_CRASH_SRC) $(PROTO_GRPC_H)
	@echo "Compiling $<..."
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Clean
clean:
	@echo "Cleaning..."
	@rm -rf $(BUILD_DIR)
	@find . -type f -name "*.o" -delete 2>/dev/null || true
	@find . -type f -name "*.pb.cc" -delete 2>/dev/null || true
	@find . -type f -name "*.pb.h" -delete 2>/dev/null || true
	@find . -type f -name "*.grpc.pb.cc" -delete 2>/dev/null || true
	@find . -type f -name "*.grpc.pb.h" -delete 2>/dev/null || true
	@echo "Clean complete."