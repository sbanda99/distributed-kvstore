// Blocking Client Implementation

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <cstdint>
#include <grpcpp/grpcpp.h>
#include "../common/config.h"

// Proto headers
#include "kvstore.grpc.pb.h"
#include "kvstore.pb.h"

namespace kvstore {

// Blocking Client Implementation
class BlockingClientImpl {
public:
    // Create a Blocking client implementation with the given configuration and client ID.
    BlockingClientImpl(const Config& config, int32_t client_id);
    ~BlockingClientImpl();
    
    // Read a key using the blocking protocol.
    // Acquires locks, reads from quorum, then releases locks.
    bool Read(const std::string& key, std::string& value);
    
    // Write a key-value pair using the blocking protocol.
    // Acquires locks, writes to quorum, then releases locks.
    bool Write(const std::string& key, const std::string& value);
    
    // Get the client's current logical timestamp.
    int64_t GetCurrentTimestamp() const;

private:
    Config config_;              // Configuration (servers, quorums, etc.)
    int32_t client_id_;          // Unique client identifier
    
    mutable std::mutex timestamp_mutex_;  // Protects client_timestamp_
    int64_t client_timestamp_;            // Client's logical clock
    
    // Create a gRPC stub for communicating with a server.
    std::unique_ptr<BlockingService::Stub> CreateStub(const ServerInfo& server);
    
    // Response from a lock acquisition request.
    struct LockResponse {
        bool granted;            // Whether the lock was granted
        int64_t timestamp;       // Server timestamp
    };
    
    // Request a lock from a server.
    LockResponse AcquireLockFromServer(const std::string& key,
                                       std::unique_ptr<BlockingService::Stub>& stub);
    
    // Release a lock on a server.
    bool ReleaseLockFromServer(const std::string& key,
                               std::unique_ptr<BlockingService::Stub>& stub);
    
    // Response from a read operation on a single server.
    struct ReadResponse {
        std::string value;
        int64_t timestamp;
        bool success;
    };
    
    // Read a key from a single server (must hold lock first).
    ReadResponse ReadFromServer(const std::string& key,
                                std::unique_ptr<BlockingService::Stub>& stub);
    
    // Write a key-value pair to a single server (must hold lock first).
    bool WriteToServer(const std::string& key, const std::string& value,
                      int64_t timestamp,
                      std::unique_ptr<BlockingService::Stub>& stub);
    
    // Update the client's logical timestamp.
    void UpdateTimestamp(int64_t timestamp);
};

}
