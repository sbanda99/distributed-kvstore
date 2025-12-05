// This implements the ABD protocol for server-side storage.
#pragma once

#include <string>
#include <map>
#include <mutex>
#include <cstdint>

namespace kvstore {


// Each server maintains its own in-memory key-value store with timestamps.
// The client-side logic (in abd_client_impl.cpp) implements the full ABD algorithm
// including the two-phase read and write operations.
class ABDProtocol {
public:
    ABDProtocol();
    ~ABDProtocol();
    
    // Result of a read operation.
    // Contains the value, its timestamp, and whether the operation succeeded.
    struct ReadResult {
        std::string value;      // The stored value (empty if key doesn't exist)
        int64_t timestamp;      // Timestamp associated with this value
        bool success;           // Whether the read operation succeeded
    };
    
    // Result of a write operation.
    // Contains success status and the final timestamp assigned to the value.
    struct WriteResult {
        bool success;           // Whether the write operation succeeded
        int64_t timestamp;      // Final timestamp assigned to the value
    };
    
    // Read the value for a key.
    // This is the server-side read operation. The client implements the full
    // ABD two-phase read (read from quorum, then write back the max value).
    // @param key The key to read
    // @param client_timestamp Client's timestamp (used for ordering, but server
    //                         returns its own stored value regardless)
    // @return ReadResult containing value, timestamp, and success status
    ReadResult Read(const std::string& key, int64_t client_timestamp);
    
    // Write a value for a key.
    // The server accepts the write and assigns a timestamp that is at least
    // as large as the client's timestamp. This ensures monotonicity.
    // @param key The key to write
    // @param value The value to store
    // @param client_timestamp Client's timestamp for this write
    // @return WriteResult containing success status and final timestamp
    WriteResult Write(const std::string& key, const std::string& value, int64_t client_timestamp);
    
    // Get the current timestamp for a key (for debugging).
    // @param key The key to check
    // @return Timestamp of the key, or 0 if key doesn't exist
    int64_t GetTimestamp(const std::string& key) const;
    
    // Get the value for a key (for debugging).
    // @param key The key to check
    // @return Value of the key, or empty string if key doesn't exist
    std::string GetValue(const std::string& key) const;

private:
    // Internal storage entry for a key-value pair.
    // Stores both the value and its timestamp.
    struct ValueEntry {
        std::string value;      // The actual value
        int64_t timestamp;      // Timestamp for ordering
        
        ValueEntry() : timestamp(0) {}
        ValueEntry(const std::string& v, int64_t ts) : value(v), timestamp(ts) {}
    };
    
    mutable std::mutex mutex_;                    // Protects the store from concurrent access
    std::map<std::string, ValueEntry> store_;      // In-memory key-value store
    int64_t last_timestamp_;                      // Last timestamp generated (for monotonicity)
    
    // Generate a new timestamp that is strictly greater than all previous timestamps.
    // This ensures we can always order operations correctly.
    // @return New monotonically increasing timestamp
    int64_t GenerateTimestamp();
};

} // namespace kvstore
