#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>

// Include generated proto headers
#include "kvstore.pb.h"
#include "kvstore.grpc.pb.h"
#include "../protocol/abd.h"
#include "../common/config.h"
#include "../common/utils.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using kvstore::ABDService;
using kvstore::ABDReadRequest;
using kvstore::ABDReadResponse;
using kvstore::ABDWriteRequest;
using kvstore::ABDWriteResponse;

// gRPC service implementation for ABD protocol.
// This class implements the ABDService interface defined in kvstore.proto.
// It handles incoming read and write requests from clients.
class ABDServiceImpl final : public ABDService::Service {
public:
    ABDServiceImpl() : protocol_(std::make_unique<kvstore::ABDProtocol>()) {}
    
    // Handles a read request from a client.
    // The client sends a key and its timestamp. The server returns the
    // stored value and timestamp for that key (if it exists).
    Status Read(ServerContext* context, const ABDReadRequest* request,
                ABDReadResponse* response) override {
        std::string key = request->key();
        int64_t client_timestamp = request->timestamp();
        
        // Log incoming request
        std::string peer = context->peer();
        std::cout << "[SERVER] Read request from " << peer 
                  << " for key='" << key << "' (client_ts=" << client_timestamp << ")" << std::endl;
        
        auto result = protocol_->Read(key, client_timestamp);
        
        response->set_value(result.value);
        response->set_timestamp(result.timestamp);
        response->set_success(result.success);
        
        std::cout << "[SERVER] Read response: value='" << result.value 
                  << "', ts=" << result.timestamp << ", success=" << result.success << std::endl;
        
        return Status::OK;
    }
    
    // Handles a write request from a client.
    // The client sends a key, value, and timestamp. The server stores
    // the value with a timestamp that is at least as large as the client's
    // timestamp (to ensure monotonicity).
    Status Write(ServerContext* context, const ABDWriteRequest* request,
                 ABDWriteResponse* response) override {
        std::string key = request->key();
        std::string value = request->value();
        int64_t client_timestamp = request->timestamp();
        
        // Log incoming request
        std::string peer = context->peer();
        std::cout << "[SERVER] Write request from " << peer 
                  << " for key='" << key << "' value='" << value 
                  << "' (client_ts=" << client_timestamp << ")" << std::endl;
        
        auto result = protocol_->Write(key, value, client_timestamp);
        
        response->set_success(result.success);
        response->set_timestamp(result.timestamp);
        
        std::cout << "[SERVER] Write response: ts=" << result.timestamp 
                  << ", success=" << result.success << std::endl;
        
        return Status::OK;
    }

private:
    std::unique_ptr<kvstore::ABDProtocol> protocol_;  // ABD protocol implementation
};

// Start and run the gRPC server.
// @param server_address Address to bind to 
// @param server_id Unique identifier for this server
void RunServer(const std::string& server_address, int32_t server_id) {
    ABDServiceImpl service;
    
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
    
    std::cout << " ABD Server successfully started and listening on " << server_address 
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
    
    std::cout << "Starting ABD Server..." << std::endl;
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

