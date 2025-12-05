// This file implements the Blocking Protocol Server
#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>

#include "kvstore.pb.h"
#include "kvstore.grpc.pb.h"
#include "../protocol/blocking.h"
#include "../common/config.h"
#include "../common/utils.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using kvstore::BlockingService;
using kvstore::BlockingLockRequest;
using kvstore::BlockingLockResponse;
using kvstore::BlockingReadRequest;
using kvstore::BlockingReadResponse;
using kvstore::BlockingWriteRequest;
using kvstore::BlockingWriteResponse;
using kvstore::BlockingUnlockRequest;
using kvstore::BlockingUnlockResponse;

// gRPC service implementation for Blocking protocol.
// This class implements the BlockingService interface defined in kvstore.proto.
// It handles lock acquisition, read, write, and lock release requests.
class BlockingServiceImpl final : public BlockingService::Service {
public:
    BlockingServiceImpl() : protocol_(std::make_unique<kvstore::BlockingProtocol>()) {}
    
    // Handles a lock acquisition request.
    // Client requests a lock for a key. Server grants it if:
    // - Key is not locked, OR
    // - Lock has timed out, OR
    // - Same client already holds the lock
    Status AcquireLock(ServerContext* context, const BlockingLockRequest* request,
                      BlockingLockResponse* response) override {
        std::string key = request->key();
        int32_t client_id = request->client_id();
        
        std::string peer = context->peer();
        std::cout << "[SERVER] AcquireLock request from " << peer 
                  << " (client_id=" << client_id << ") for key='" << key << "'" << std::endl;
        
        auto result = protocol_->AcquireLock(key, client_id);
        
        response->set_granted(result.granted);
        response->set_timestamp(result.timestamp);
        
        std::cout << "[SERVER] AcquireLock response: granted=" << result.granted 
                  << ", ts=" << result.timestamp << std::endl;
        
        return Status::OK;
    }
    
    // Handles a read request.
    // Client must hold the lock for this key. Server returns the stored
    // value and timestamp.
    Status Read(ServerContext* context, const BlockingReadRequest* request,
                BlockingReadResponse* response) override {
        std::string key = request->key();
        int32_t client_id = request->client_id();
        
        std::string peer = context->peer();
        std::cout << "[SERVER] Read request from " << peer 
                  << " (client_id=" << client_id << ") for key='" << key << "'" << std::endl;
        
        auto result = protocol_->Read(key, client_id);
        
        response->set_value(result.value);
        response->set_timestamp(result.timestamp);
        response->set_success(result.success);
        
        std::cout << "[SERVER] Read response: value='" << result.value 
                  << "', ts=" << result.timestamp << ", success=" << result.success << std::endl;
        
        return Status::OK;
    }
    
    // Handles a write request.
    // Client must hold the lock for this key. Server stores the value
    // with an appropriate timestamp.
    Status Write(ServerContext* context, const BlockingWriteRequest* request,
                 BlockingWriteResponse* response) override {
        std::string key = request->key();
        std::string value = request->value();
        int64_t client_timestamp = request->timestamp();
        int32_t client_id = request->client_id();
        
        std::string peer = context->peer();
        std::cout << "[SERVER] Write request from " << peer 
                  << " (client_id=" << client_id << ") for key='" << key 
                  << "' value='" << value << "' (client_ts=" << client_timestamp << ")" << std::endl;
        
        auto result = protocol_->Write(key, value, client_timestamp, client_id);
        
        response->set_success(result.success);
        response->set_timestamp(result.timestamp);
        
        std::cout << "[SERVER] Write response: ts=" << result.timestamp 
                  << ", success=" << result.success << std::endl;
        
        return Status::OK;
    }
    
    // Handles a lock release request.
    // Client releases the lock it holds for a key.
    Status ReleaseLock(ServerContext* context, const BlockingUnlockRequest* request,
                      BlockingUnlockResponse* response) override {
        std::string key = request->key();
        int32_t client_id = request->client_id();
        
        std::string peer = context->peer();
        std::cout << "[SERVER] ReleaseLock request from " << peer 
                  << " (client_id=" << client_id << ") for key='" << key << "'" << std::endl;
        
        bool success = protocol_->ReleaseLock(key, client_id);
        response->set_success(success);
        
        std::cout << "[SERVER] ReleaseLock response: success=" << success << std::endl;
        
        return Status::OK;
    }

private:
    std::unique_ptr<kvstore::BlockingProtocol> protocol_;  // Blocking protocol implementation
};

// Start and run the gRPC server.
// @param server_address Address to bind to 
// @param server_id Unique identifier for this server
void RunServer(const std::string& server_address, int32_t server_id) {
    BlockingServiceImpl service;
    
    // Enable gRPC features
    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    
    // Build and start the server
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    
    std::unique_ptr<Server> server(builder.BuildAndStart());
    if (!server) {
        std::cerr << "ERROR: Failed to start server on " << server_address << std::endl;
        std::cerr << "  Check if port is already in use or if address is invalid" << std::endl;
        return;
    }
    
    std::cout << " Blocking Server successfully started and listening on " << server_address 
              << " (Server ID: " << server_id << ")" << std::endl;
    std::cout << "  Ready to accept connections..." << std::endl;
    
    // Block until server is shut down
    server->Wait();
}

int main(int argc, char** argv) {
    // Default values
    std::string config_file = "";
    int32_t server_id = 0;
    int32_t port = 5001;
    std::string host = "0.0.0.0";
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            config_file = argv[++i];
        } else if (arg == "--server-id" && i + 1 < argc) {
            server_id = std::stoi(argv[++i]);
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        }
    }
    
    // Load configuration from file if provided
    kvstore::Config config;
    std::string config_hostname = "";
    if (!config_file.empty() && config.LoadFromFile(config_file)) {
        auto server_info = config.GetServer(server_id);
        if (server_info.port != 0) {
            port = server_info.port;
            config_hostname = server_info.host;  // Store for logging, but don't use for binding
        }
    }
    
    // IMPORTANT: Always bind to 0.0.0.0 (all interfaces) so server can accept
    // connections from any network interface. The config hostname is only used
    // by clients to know where to connect - servers should listen on all interfaces.
    std::string bind_address = kvstore::FormatAddress("0.0.0.0", port);
    
    std::cout << "Starting Blocking Server..." << std::endl;
    std::cout << "  Server ID: " << server_id << std::endl;
    std::cout << "  Binding to: " << bind_address << " (listening on all interfaces)" << std::endl;
    if (!config_hostname.empty()) {
        std::cout << "  Config hostname: " << config_hostname 
                  << " (clients should connect to this)" << std::endl;
    }
    std::cout << "  Port: " << port << std::endl;
    
    RunServer(bind_address, server_id);
    
    return 0;
}
