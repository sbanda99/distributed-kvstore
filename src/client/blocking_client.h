// Blocking Client Public Interface

#pragma once

#include <string>
#include <memory>
#include <cstdint>
#include "../common/config.h"

namespace kvstore {

class BlockingClientImpl;

// Blocking Protocol Client
class BlockingClient {
public:
    // Create a Blocking client with the given configuration and client ID.
    // @param config Configuration object containing server addresses and quorum sizes
    // @param client_id Unique identifier for this client (used for lock ownership)
    BlockingClient(const Config& config, int32_t client_id);
    ~BlockingClient();
    
    // Read the value for a key.
    // The client must acquire locks from a quorum of servers before reading.
    // This operation may block if locks are held by other clients.
    // @param key The key to read
    // @param value Output parameter - will contain the value if read succeeds
    // @return true if read succeeded, false otherwise
    bool Read(const std::string& key, std::string& value);
    
    // Write a value for a key.
    // The client must acquire locks from a quorum of servers before writing.
    // This operation may block if locks are held by other clients.
    // @param key The key to write
    // @param value The value to store
    // @return true if write succeeded, false otherwise
    bool Write(const std::string& key, const std::string& value);
    
    // Get the client's current logical timestamp.
    // @return Current client timestamp
    int64_t GetCurrentTimestamp() const;

private:
    std::unique_ptr<BlockingClientImpl> impl_;  // PIMPL - hides implementation details
};

}
