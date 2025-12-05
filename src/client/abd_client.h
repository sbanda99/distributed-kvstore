// This file implements the ABD Client Public Interface

#pragma once

#include <string>
#include <memory>
#include <cstdint>
#include "../common/config.h"

namespace kvstore {

class ABDClientImpl;

// ABD Protocol Client
class ABDClient {
public:
    // Create an ABD client with the given configuration.
    // @param config Configuration object containing server addresses and quorum sizes
    ABDClient(const Config& config);
    ~ABDClient();
    
    // Read the value for a key.
    // Implements the ABD two-phase read protocol:
    // - Phase 1: Read from a quorum of servers, get their values and timestamps
    // - Phase 2: Find the value with maximum timestamp, write it back to servers
    // This ensures linearizability - all clients see the same ordering of operations.
    // @param key The key to read
    // @param value Output parameter - will contain the value if read succeeds
    // @return true if read succeeded, false otherwise
    bool Read(const std::string& key, std::string& value);
    
    // Write a value for a key.
    // Implements the ABD write protocol:
    // - Write to a quorum of servers with a new timestamp
    // - Once a write quorum acknowledges, the write is considered committed
    // @param key The key to write
    // @param value The value to store
    // @return true if write succeeded, false otherwise
    bool Write(const std::string& key, const std::string& value);
    
    // Get the client's current logical timestamp.
    // The client maintains a logical clock that is updated based on
    // timestamps received from servers. This ensures the client's
    // timestamps are always increasing.
    // @return Current client timestamp
    int64_t GetCurrentTimestamp() const;

private:
    std::unique_ptr<ABDClientImpl> impl_;  // PIMPL - hides implementation details
};

}
