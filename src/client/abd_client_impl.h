// This file contains the actual implementation of the ABD client protocol.

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

// ABD Client Implementation
class ABDClientImpl {
public:
    ABDClientImpl(const Config& config);
    ~ABDClientImpl();
    
    // Read a key using the ABD two-phase read protocol.
    bool Read(const std::string& key, std::string& value);
    
    // Write a key-value pair using the ABD write protocol.
    bool Write(const std::string& key, const std::string& value);
    
    // Get the client's current logical timestamp.
    int64_t GetCurrentTimestamp() const;

private:
    Config config_;  // Configuration (servers, quorums, etc.)
    
    mutable std::mutex timestamp_mutex_;  // Protects client_timestamp_
    int64_t client_timestamp_;            // Client's logical clock
    
    // Create a gRPC stub for communicating with a server.
    // @param server Server information (host, port)
    // @return gRPC stub for this server
    std::unique_ptr<ABDService::Stub> CreateStub(const ServerInfo& server);
    
    // Response from a read operation on a single server.
    struct ReadResponse {
        std::string value;      // Value returned by server
        int64_t timestamp;      // Timestamp of the value
        bool success;           // Whether the RPC succeeded
    };
    
    // Read a key from a single server.
    // @param key Key to read
    // @param stub gRPC stub for the server
    // @return ReadResponse with value, timestamp, and success status
    ReadResponse ReadFromServer(const std::string& key, 
                                std::unique_ptr<ABDService::Stub>& stub);
    
    // Write a key-value pair to a single server.
    // @param key Key to write
    // @param value Value to write
    // @param timestamp Timestamp for this write
    // @param stub gRPC stub for the server
    // @return true if write succeeded, false otherwise
    bool WriteToServer(const std::string& key, const std::string& value, 
                      int64_t timestamp,
                      std::unique_ptr<ABDService::Stub>& stub);
    
    // Update the client's logical timestamp based on a server response.
    // The client timestamp is always kept greater than or equal to any
    // timestamp it has seen from servers. This ensures monotonicity.
    // @param timestamp Timestamp received from a server
    void UpdateTimestamp(int64_t timestamp);
};

}
