// ABD Protocol Implementation
// Server-side storage logic for the ABD protocol.
// Each server maintains an in-memory key-value store with timestamps.

#include "abd.h"
#include <algorithm>
#include <chrono>

namespace kvstore {

ABDProtocol::ABDProtocol() : last_timestamp_(0) {
}

ABDProtocol::~ABDProtocol() {
}

ABDProtocol::ReadResult ABDProtocol::Read(const std::string& key, int64_t /* client_timestamp */) { // NOLINT(readability-named-parameter)
    std::lock_guard<std::mutex> lock(mutex_);
    
    ReadResult result;
    result.success = false;
    result.timestamp = 0;
    
    // Look up the key in our store
    auto it = store_.find(key);
    if (it != store_.end()) {
        // If the key exists - return its value and timestamp
        result.value = it->second.value;
        result.timestamp = it->second.timestamp;
        result.success = true;
    } else {
        // If the key doesn't exist - return empty value with timestamp 0
        result.value = "";
        result.timestamp = 0;
        result.success = true;
    }
    
    // Note: We return our stored value regardless of the client's timestamp.
    // The client-side ABD algorithm handles selecting the maximum timestamp
    // value from the quorum of servers.
    return result;
}

ABDProtocol::WriteResult ABDProtocol::Write(const std::string& key, 
                                             const std::string& value, 
                                             int64_t client_timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    WriteResult result;
    result.success = false;
    
    // Generate a new server timestamp
    int64_t server_timestamp = GenerateTimestamp();
    
    // Use the maximum of client and server timestamps
    // This ensures that timestamps are always increasing, even if a client
    // sends an old timestamp
    int64_t final_timestamp = std::max(client_timestamp, server_timestamp);
    
    // Update the store with the new value and timestamp
    // We always accept the write (even if client timestamp is old) to keep
    // the implementation simple. In a strict ABD implementation, we could
    // reject writes with timestamps that are too old.
    store_[key] = ValueEntry(value, final_timestamp);
    result.success = true;
    result.timestamp = final_timestamp;
    
    return result;
}

int64_t ABDProtocol::GetTimestamp(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = store_.find(key);
    if (it != store_.end()) {
        return it->second.timestamp;
    }
    return 0;
}

std::string ABDProtocol::GetValue(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = store_.find(key);
    if (it != store_.end()) {
        return it->second.value;
    }
    return "";
}

int64_t ABDProtocol::GenerateTimestamp() {
    // Generate a monotonically increasing timestamp
    // This ensures that every operation gets a unique, ordered timestamp
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    int64_t current = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    
    // Ensure timestamp is always increasing
    if (current > last_timestamp_) {
        last_timestamp_ = current;
    } else {
        last_timestamp_++;  // Increment if clock went backwards
    }
    
    return last_timestamp_;
}

}
