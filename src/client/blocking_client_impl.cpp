// This file implements the Blocking Client Implementation

#include "blocking_client_impl.h"
#include "../common/utils.h"
#include <algorithm>
#include <future>
#include <chrono>
#include <iostream>

namespace kvstore {

BlockingClientImpl::BlockingClientImpl(const Config& config, int32_t client_id)
    : config_(config), client_id_(client_id), client_timestamp_(GetCurrentTimestamp()) {
}

BlockingClientImpl::~BlockingClientImpl() {
}

std::unique_ptr<BlockingService::Stub> BlockingClientImpl::CreateStub(const ServerInfo& server) {
    std::string address = server.GetAddress();
    auto channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
    return BlockingService::NewStub(channel);
}

BlockingClientImpl::LockResponse BlockingClientImpl::AcquireLockFromServer(
    const std::string& key,
    std::unique_ptr<BlockingService::Stub>& stub) {
    
    LockResponse response;
    response.granted = false;
    
    grpc::ClientContext context;
    BlockingLockRequest request;
    BlockingLockResponse reply;
    
    request.set_key(key);
    request.set_client_id(client_id_);
    
    grpc::Status status = stub->AcquireLock(&context, request, &reply);
    
    if (status.ok()) {
        response.granted = reply.granted();
        response.timestamp = reply.timestamp();
    }
    
    return response;
}

bool BlockingClientImpl::ReleaseLockFromServer(
    const std::string& key,
    std::unique_ptr<BlockingService::Stub>& stub) {
    
    grpc::ClientContext context;
    BlockingUnlockRequest request;
    BlockingUnlockResponse reply;
    
    request.set_key(key);
    request.set_client_id(client_id_);
    
    grpc::Status status = stub->ReleaseLock(&context, request, &reply);
    
    return status.ok() && reply.success();
}

BlockingClientImpl::ReadResponse BlockingClientImpl::ReadFromServer(
    const std::string& key,
    std::unique_ptr<BlockingService::Stub>& stub) {
    
    ReadResponse response;
    response.success = false;
    
    grpc::ClientContext context;
    BlockingReadRequest request;
    BlockingReadResponse reply;
    
    request.set_key(key);
    request.set_client_id(client_id_);
    
    grpc::Status status = stub->Read(&context, request, &reply);
    
    if (status.ok()) {
        response.value = reply.value();
        response.timestamp = reply.timestamp();
        response.success = reply.success();
    }
    
    return response;
}

bool BlockingClientImpl::WriteToServer(const std::string& key,
                                   const std::string& value,
                                   int64_t timestamp,
                                   std::unique_ptr<BlockingService::Stub>& stub) {
    grpc::ClientContext context;
    BlockingWriteRequest request;
    BlockingWriteResponse reply;
    
    request.set_key(key);
    request.set_value(value);
    request.set_timestamp(timestamp);
    request.set_client_id(client_id_);
    
    grpc::Status status = stub->Write(&context, request, &reply);
    
    if (status.ok() && reply.success()) {
        UpdateTimestamp(reply.timestamp());
        return true;
    }
    
    return false;
}

void BlockingClientImpl::UpdateTimestamp(int64_t timestamp) {
    std::lock_guard<std::mutex> lock(timestamp_mutex_);
    if (timestamp > client_timestamp_) {
        client_timestamp_ = timestamp;
    }
    client_timestamp_++;
}

int64_t BlockingClientImpl::GetCurrentTimestamp() const {
    std::lock_guard<std::mutex> lock(timestamp_mutex_);
    return client_timestamp_;
}

bool BlockingClientImpl::Read(const std::string& key, std::string& value) {
    int32_t read_quorum = config_.GetReadQuorum();
    const auto& servers = config_.GetServers();
    
    std::cerr << "\n[BLOCKING READ]" << std::endl;
    std::cerr << "[BLOCKING READ] Starting read for key='" << key << "'" << std::endl;
    std::cerr << "[BLOCKING READ] Need R=" << read_quorum << " locks from " 
              << servers.size() << " servers" << std::endl;
    
    if (static_cast<size_t>(read_quorum) > servers.size()) {
        std::cerr << "[BLOCKING READ] ✗ Error: Read quorum larger than number of servers" << std::endl;
        return false;
    }
    
    // PHASE 1: Acquire locks
    std::cerr << "[BLOCKING READ Phase 1] Requesting locks from " << servers.size() << " servers..." << std::endl;
    
    std::vector<std::unique_ptr<BlockingService::Stub>> stubs;
    for (const auto& server : servers) {
        stubs.push_back(CreateStub(server));
    }
    
    // Send lock requests to all servers in parallel
    std::vector<std::future<LockResponse>> lock_futures;
    for (size_t i = 0; i < servers.size(); i++) {
        lock_futures.push_back(std::async(std::launch::async,
            &BlockingClientImpl::AcquireLockFromServer, this, key, std::ref(stubs[i])));
    }
    
    // Collect lock grants until we have a read quorum
    std::vector<size_t> locked_server_indices;
    for (size_t i = 0; i < lock_futures.size(); i++) {
        auto lock_response = lock_futures[i].get();
        if (lock_response.granted) {
            locked_server_indices.push_back(i);
            std::cerr << "[BLOCKING READ Phase 1] Lock granted from server " << i 
                      << " (" << locked_server_indices.size() << "/" << read_quorum << ")" << std::endl;
            if (static_cast<int32_t>(locked_server_indices.size()) >= read_quorum) {
                std::cerr << "[BLOCKING READ Phase 1] Lock quorum achieved! (" 
                          << locked_server_indices.size() << " locks)" << std::endl;
                break;
            }
        } else {
            std::cerr << "[BLOCKING READ Phase 1] Lock denied from server " << i 
                      << " (may be held by another client)" << std::endl;
        }
    }
    
    // If we didn't get enough locks, release what we got and fail
    if (static_cast<int32_t>(locked_server_indices.size()) < read_quorum) {
        std::cerr << "[BLOCKING READ Phase 1] Only got " << locked_server_indices.size() 
                  << " locks, need " << read_quorum << " - releasing locks..." << std::endl;
        for (size_t idx : locked_server_indices) {
            ReleaseLockFromServer(key, stubs[idx]);
        }
        std::cerr << "[BLOCKING READ] Failed: Could not acquire read quorum locks" << std::endl;
        return false;
    }
    
    // PHASE 2: Read from locked servers
    std::cerr << "[BLOCKING READ Phase 2] Reading from " << locked_server_indices.size() 
              << " locked servers..." << std::endl;
    
    std::vector<ReadResponse> responses;
    for (size_t idx : locked_server_indices) {
        auto response = ReadFromServer(key, stubs[idx]);
        if (response.success) {
            responses.push_back(response);
            std::cerr << "[BLOCKING READ Phase 2] Read from server " << idx 
                      << " (ts=" << response.timestamp << ")" << std::endl;
        } else {
            std::cerr << "[BLOCKING READ Phase 2] Read failed from server " << idx << std::endl;
        }
    }
    
    if (responses.empty()) {
        std::cerr << "[BLOCKING READ Phase 2] No successful reads - releasing locks..." << std::endl;
        for (size_t idx : locked_server_indices) {
            ReleaseLockFromServer(key, stubs[idx]);
        }
        std::cerr << "[BLOCKING READ] Failed: Could not read from locked servers" << std::endl;
        return false;
    }
    
    // PHASE 3: Find maximum timestamp value
    auto max_response = std::max_element(responses.begin(), responses.end(),
        [](const ReadResponse& a, const ReadResponse& b) {
            return a.timestamp < b.timestamp;
        });
    
    value = max_response->value;
    std::cerr << "[BLOCKING READ Phase 3] Found max timestamp: " << max_response->timestamp 
              << " (value='" << value << "')" << std::endl;
    
    // PHASE 4: Release locks
    std::cerr << "[BLOCKING READ Phase 4] Releasing " << locked_server_indices.size() << " locks..." << std::endl;
    for (size_t idx : locked_server_indices) {
        if (ReleaseLockFromServer(key, stubs[idx])) {
            std::cerr << "[BLOCKING READ Phase 4] Lock released from server " << idx << std::endl;
        } else {
            std::cerr << "[BLOCKING READ Phase 4] Failed to release lock from server " << idx << std::endl;
        }
    }
    
    std::cerr << "[BLOCKING READ] Read complete, value='" << value << "'" << std::endl;
    
    return true;
}

bool BlockingClientImpl::Write(const std::string& key, const std::string& value) {
    int32_t write_quorum = config_.GetWriteQuorum();
    const auto& servers = config_.GetServers();
    
    std::cerr << "\n[BLOCKING WRITE]" << std::endl;
    std::cerr << "[BLOCKING WRITE] Starting write for key='" << key << "'" << std::endl;
    std::cerr << "[BLOCKING WRITE] Need W=" << write_quorum << " locks from " 
              << servers.size() << " servers" << std::endl;
    
    if (static_cast<size_t>(write_quorum) > servers.size()) {
        std::cerr << "[BLOCKING WRITE] ✗ Error: Write quorum larger than number of servers" << std::endl;
        return false;
    }
    
    // PHASE 1: Acquire locks
    std::cerr << "[BLOCKING WRITE Phase 1] Requesting locks from " << servers.size() << " servers..." << std::endl;
    
    std::vector<std::unique_ptr<BlockingService::Stub>> stubs;
    for (const auto& server : servers) {
        stubs.push_back(CreateStub(server));
    }
    
    // Send lock requests to all servers in parallel
    std::vector<std::future<LockResponse>> lock_futures;
    for (size_t i = 0; i < servers.size(); i++) {
        lock_futures.push_back(std::async(std::launch::async,
            &BlockingClientImpl::AcquireLockFromServer, this, key, std::ref(stubs[i])));
    }
    
    // Collect lock grants until we have a write quorum
    std::vector<size_t> locked_server_indices;
    for (size_t i = 0; i < lock_futures.size(); i++) {
        auto lock_response = lock_futures[i].get();
        if (lock_response.granted) {
            locked_server_indices.push_back(i);
            std::cerr << "[BLOCKING WRITE Phase 1] Lock granted from server " << i 
                      << " (" << locked_server_indices.size() << "/" << write_quorum << ")" << std::endl;
            if (static_cast<int32_t>(locked_server_indices.size()) >= write_quorum) {
                std::cerr << "[BLOCKING WRITE Phase 1] Lock quorum achieved! (" 
                          << locked_server_indices.size() << " locks)" << std::endl;
                break;
            }
        } else {
            std::cerr << "[BLOCKING WRITE Phase 1] Lock denied from server " << i 
                      << " (may be held by another client)" << std::endl;
        }
    }
    
    // If we didn't get enough locks, release what we got and fail
    if (static_cast<int32_t>(locked_server_indices.size()) < write_quorum) {
        std::cerr << "[BLOCKING WRITE Phase 1] Only got " << locked_server_indices.size() 
                  << " locks, need " << write_quorum << " - releasing locks..." << std::endl;
        for (size_t idx : locked_server_indices) {
            ReleaseLockFromServer(key, stubs[idx]);
        }
        std::cerr << "[BLOCKING WRITE] Failed: Could not acquire write quorum locks" << std::endl;
        return false;
    }
    
    // PHASE 2: Write to locked servers
    int64_t timestamp = GetCurrentTimestamp() + 1;
    UpdateTimestamp(timestamp);
    
    std::cerr << "[BLOCKING WRITE Phase 2] Writing to " << locked_server_indices.size() 
              << " locked servers (ts=" << timestamp << ")..." << std::endl;
    
    int32_t written = 0;
    for (size_t idx : locked_server_indices) {
        if (WriteToServer(key, value, timestamp, stubs[idx])) {
            written++;
            std::cerr << "[BLOCKING WRITE Phase 2] Write " << written << "/" << write_quorum 
                      << " successful (server " << idx << ")" << std::endl;
        } else {
            std::cerr << "[BLOCKING WRITE Phase 2] Write failed (server " << idx << ")" << std::endl;
        }
    }
    
    // PHASE 3: Release locks
    std::cerr << "[BLOCKING WRITE Phase 3] Releasing " << locked_server_indices.size() << " locks..." << std::endl;
    for (size_t idx : locked_server_indices) {
        if (ReleaseLockFromServer(key, stubs[idx])) {
            std::cerr << "[BLOCKING WRITE Phase 3] Lock released from server " << idx << std::endl;
        } else {
            std::cerr << "[BLOCKING WRITE Phase 3] Failed to release lock from server " << idx << std::endl;
        }
    }
    
    if (written < write_quorum) {
        std::cerr << "[BLOCKING WRITE] Failed: Only " << written << " writes succeeded, need " 
                  << write_quorum << std::endl;
        return false;
    }
    
    std::cerr << "[BLOCKING WRITE] Write committed successfully" << std::endl;
    
    return true;
}

}
