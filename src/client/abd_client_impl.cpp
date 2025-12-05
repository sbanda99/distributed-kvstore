// ABD Client Implementation

#include "abd_client_impl.h"
#include "../common/utils.h"
#include <algorithm>
#include <future>
#include <chrono>
#include <iostream>

namespace kvstore {

ABDClientImpl::ABDClientImpl(const Config& config) 
    : config_(config), client_timestamp_(GetCurrentTimestamp()) {
}

ABDClientImpl::~ABDClientImpl() {
}

std::unique_ptr<ABDService::Stub> ABDClientImpl::CreateStub(const ServerInfo& server) {
    std::string address = server.GetAddress();
    std::cerr << "[CLIENT] Creating connection to server " << server.id 
              << " at " << address << std::endl;
    
    auto channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
    
    // Check channel state (non-blocking check)
    grpc_connectivity_state state = channel->GetState(true);
    if (state == GRPC_CHANNEL_SHUTDOWN) {
        std::cerr << "[CLIENT] WARNING: Channel to " << address << " is in SHUTDOWN state" << std::endl;
    }
    
    return ABDService::NewStub(channel);
}

ABDClientImpl::ReadResponse ABDClientImpl::ReadFromServer(
    const std::string& key, 
    std::unique_ptr<ABDService::Stub>& stub) {
    
    ReadResponse response;
    response.success = false;
    
    grpc::ClientContext context;
    // Set a 5-second timeout so RPCs don't hang forever
    auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
    context.set_deadline(deadline);

    ABDReadRequest request;
    ABDReadResponse reply;
    
    request.set_key(key);
    request.set_timestamp(client_timestamp_);
    
    grpc::Status status = stub->Read(&context, request, &reply);
    
    if (status.ok()) {
        response.value = reply.value();
        response.timestamp = reply.timestamp();
        response.success = reply.success();
    } else {
        // RPC failed (server down, network error, etc.)
        std::cerr << "[CLIENT] Read RPC failed: " << status.error_code() 
                  << " - " << status.error_message() << std::endl;
    }
    
    return response;
}

bool ABDClientImpl::WriteToServer(const std::string& key, 
                              const std::string& value, 
                              int64_t timestamp,
                              std::unique_ptr<ABDService::Stub>& stub) {
    grpc::ClientContext context;
    auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
    context.set_deadline(deadline);

    ABDWriteRequest request;
    ABDWriteResponse reply;
    
    request.set_key(key);
    request.set_value(value);
    request.set_timestamp(timestamp);
    
    grpc::Status status = stub->Write(&context, request, &reply);
    
    if (status.ok() && reply.success()) {
        // Update our timestamp based on server's response
        UpdateTimestamp(reply.timestamp());
        return true;
    }
    
    if (!status.ok()) {
        std::cerr << "[CLIENT] Write RPC failed: " << status.error_code() 
                  << " - " << status.error_message() << std::endl;
    }
    
    return false;
}

void ABDClientImpl::UpdateTimestamp(int64_t timestamp) {
    std::lock_guard<std::mutex> lock(timestamp_mutex_);
    // Keep client timestamp >= any timestamp we've seen from servers
    if (timestamp > client_timestamp_) {
        client_timestamp_ = timestamp;
    }
    // Increment to ensure our next timestamp is unique
    client_timestamp_++;
}

int64_t ABDClientImpl::GetCurrentTimestamp() const {
    std::lock_guard<std::mutex> lock(timestamp_mutex_);
    return client_timestamp_;
}

bool ABDClientImpl::Read(const std::string& key, std::string& value) {
    int32_t read_quorum = config_.GetReadQuorum();
    const auto& servers = config_.GetServers();
    
    std::cerr << "\n[ABD READ]" << std::endl;
    std::cerr << "[ABD READ] Starting read for key='" << key << "'" << std::endl;
    std::cerr << "[ABD READ] Need R=" << read_quorum << " responses from " 
              << servers.size() << " servers" << std::endl;
    
    if (static_cast<size_t>(read_quorum) > servers.size()) {
        std::cerr << "Error: Read quorum larger than number of servers" << std::endl;
        return false;
    }
    
    // Phase 1: Read from servers
    std::cerr << "[ABD READ Phase 1] Sending read requests to " << servers.size() << " servers..." << std::endl;
    
    std::vector<std::unique_ptr<ABDService::Stub>> stubs;
    for (const auto& server : servers) {
        std::cerr << "[CLIENT] Connecting to server " << server.id 
                  << " at " << server.GetAddress() << std::endl;
        stubs.push_back(CreateStub(server));
    }
    
    std::vector<std::future<ReadResponse>> futures;
    for (size_t i = 0; i < servers.size(); i++) {
        futures.push_back(std::async(std::launch::async, 
            &ABDClientImpl::ReadFromServer, this, key, std::ref(stubs[i])));
    }
    
    // Collect responses until we have a read quorum
    std::vector<ReadResponse> responses;
    for (size_t i = 0; i < futures.size(); i++) {
        auto response = futures[i].get();
        if (response.success) {
            responses.push_back(response);
            std::cerr << "[ABD READ Phase 1] Got response " << responses.size() 
                      << "/" << read_quorum << " (server " << i 
                      << ", ts=" << response.timestamp << ")" << std::endl;
            if (static_cast<int32_t>(responses.size()) >= read_quorum) {
                std::cerr << "[ABD READ Phase 1] Read quorum achieved! (" 
                          << responses.size() << " responses)" << std::endl;
                break;
            }
        } else {
            std::cerr << "[ABD READ Phase 1] Server " << i << " failed or returned error" << std::endl;
        }
    }
    
    if (static_cast<int32_t>(responses.size()) < read_quorum) {
        std::cerr << "[ABD READ] Error: Only got " << responses.size() 
                  << " responses, need " << read_quorum << std::endl;
        return false;
    }
    
    // Find the response with the maximum timestamp
    auto max_response = std::max_element(responses.begin(), responses.end(),
        [](const ReadResponse& a, const ReadResponse& b) {
            return a.timestamp < b.timestamp;
        });
    
    std::string max_value = max_response->value;
    int64_t max_timestamp = max_response->timestamp;
    
    std::cerr << "[ABD READ] Found max timestamp: " << max_timestamp 
              << " (value='" << max_value << "')" << std::endl;
    
    // Phase 2: Write back the maximum value
    int64_t write_timestamp = std::max(max_timestamp, GetCurrentTimestamp()) + 1;
    UpdateTimestamp(write_timestamp);
    
    std::cerr << "[ABD READ Phase 2] Writing back max value to servers (W=" 
              << config_.GetWriteQuorum() << ", ts=" << write_timestamp << ")..." << std::endl;
    
    int32_t write_quorum = config_.GetWriteQuorum();
    int32_t written = 0;
    
    for (size_t i = 0; i < stubs.size() && written < write_quorum; i++) {
        if (WriteToServer(key, max_value, write_timestamp, stubs[i])) {
            written++;
            std::cerr << "[ABD READ Phase 2] Written to server " << i 
                      << " (" << written << "/" << write_quorum << ")" << std::endl;
        }
    }
    
    if (written < write_quorum) {
        std::cerr << "[ABD READ] Error: Only wrote to " << written 
                  << " servers, need " << write_quorum << std::endl;
        return false;
    }
    
    std::cerr << "[ABD READ Phase 2] Write quorum achieved! (" << written << " writes)" << std::endl;
    std::cerr << "[ABD READ] Read complete, value='" << max_value << "'" << std::endl;
    value = max_value;
    return true;
}

bool ABDClientImpl::Write(const std::string& key, const std::string& value) {
    int32_t write_quorum = config_.GetWriteQuorum();
    const auto& servers = config_.GetServers();
    
    std::cerr << "\n[ABD WRITE]" << std::endl;
    std::cerr << "[ABD WRITE] Starting write for key='" << key << "'" << std::endl;
    std::cerr << "[ABD WRITE] Need W=" << write_quorum << " successful writes from " 
              << servers.size() << " servers" << std::endl;
    
    if (static_cast<size_t>(write_quorum) > servers.size()) {
        std::cerr << "Error: Write quorum larger than number of servers" << std::endl;
        return false;
    }
    
    // Generate a new timestamp for this write
    int64_t timestamp = GetCurrentTimestamp() + 1;
    UpdateTimestamp(timestamp);
    
    std::cerr << "[ABD WRITE] Generated timestamp: " << timestamp << std::endl;
    std::cerr << "[ABD WRITE] Sending write requests to " << servers.size() << " servers..." << std::endl;
    
    std::vector<std::unique_ptr<ABDService::Stub>> stubs;
    for (const auto& server : servers) {
        std::cerr << "[CLIENT] Connecting to server " << server.id 
                  << " at " << server.GetAddress() << std::endl;
        stubs.push_back(CreateStub(server));
    }
    
    std::vector<std::future<bool>> futures;
    for (size_t i = 0; i < servers.size(); i++) {
        futures.push_back(std::async(std::launch::async, 
            &ABDClientImpl::WriteToServer, this, key, value, timestamp, std::ref(stubs[i])));
    }
    
    // Wait for write quorum acknowledgments
    int32_t written = 0;
    for (size_t i = 0; i < futures.size(); i++) {
        if (futures[i].get()) {
            written++;
            std::cerr << "[ABD WRITE] Got acknowledgment from server " << i 
                      << " (" << written << "/" << write_quorum << ")" << std::endl;
            if (written >= write_quorum) {
                std::cerr << "[ABD WRITE] Write quorum achieved! (" 
                          << written << " acknowledgments)" << std::endl;
                break;
            }
        } else {
            std::cerr << "[ABD WRITE] Server " << i << " failed or returned error" << std::endl;
        }
    }
    
    if (written < write_quorum) {
        std::cerr << "[ABD WRITE] Error: Only got " << written 
                  << " acknowledgments, need " << write_quorum << std::endl;
        return false;
    }
    
    std::cerr << "[ABD WRITE] Write committed successfully" << std::endl;
    return true;
}

}
