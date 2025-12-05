// This file implements the Blocking Protocol Implementation 
// Unlike ABD, this protocol can block clients if another client holds a lock
// or if a client crashes while holding a lock.

#pragma once

#include <string>
#include <map>
#include <mutex>
#include <cstdint>
#include <chrono>

namespace kvstore {

// Blocking Protocol implementation for server-side storage.
// Each server maintains:
// - An in-memory key-value store (like ABD)
// - A lock table tracking which client holds each lock
// Clients must acquire locks before reading or writing.
class BlockingProtocol {
public:
    BlockingProtocol();
    ~BlockingProtocol();
    
    // Result of a lock acquisition attempt.
    struct LockResult {
        bool granted;           // Whether the lock was granted
        int64_t timestamp;      // Server timestamp (for ordering)
    };
    
    // Result of a read operation.
    struct ReadResult {
        std::string value;      // The stored value
        int64_t timestamp;      // Timestamp of the value
        bool success;           // Whether the read succeeded (fails if no lock)
    };
    
    // Result of a write operation.
    struct WriteResult {
        bool success;           // Whether the write succeeded (fails if no lock)
        int64_t timestamp;     // Final timestamp assigned to the value
    };
    
    // Attempt to acquire a lock for a key.
    // The lock is granted if:
    // - The key is not currently locked, OR
    // - The current lock has timed out, OR
    // - The same client already holds the lock (re-entrant)
    // @param key The key to lock
    // @param client_id Unique identifier for the client requesting the lock
    // @return LockResult indicating if lock was granted
    LockResult AcquireLock(const std::string& key, int32_t client_id);
    
    // Release a lock for a key.
    // Only succeeds if the calling client actually holds the lock.
    // @param key The key to unlock
    // @param client_id ID of the client releasing the lock
    // @return true if lock was released, false if client didn't hold the lock
    bool ReleaseLock(const std::string& key, int32_t client_id);
    
    // Read the value for a key.
    // Requires that the client holds the lock for this key.
    // @param key The key to read
    // @param client_id ID of the client performing the read
    // @return ReadResult containing value, timestamp, and success status
    ReadResult Read(const std::string& key, int32_t client_id);
    
    // Write a value for a key.
    // Requires that the client holds the lock for this key.
    // @param key The key to write
    // @param value The value to store
    // @param client_timestamp Client's timestamp for this write
    // @param client_id ID of the client performing the write
    // @return WriteResult containing success status and final timestamp
    WriteResult Write(const std::string& key, const std::string& value, 
                     int64_t client_timestamp, int32_t client_id);
    
    // Utility methods for debugging/testing
    int64_t GetTimestamp(const std::string& key) const;
    std::string GetValue(const std::string& key) const;
    bool IsLocked(const std::string& key) const;
    int32_t GetLockOwner(const std::string& key) const;

private:
    // Internal storage entry for a key-value pair.
    struct ValueEntry {
        std::string value;
        int64_t timestamp;
        
        ValueEntry() : timestamp(0) {}
        ValueEntry(const std::string& v, int64_t ts) : value(v), timestamp(ts) {}
    };
    
    // Lock entry tracking who holds a lock and when it was acquired.
    struct LockEntry {
        int32_t owner_id;       // ID of the client holding the lock
        std::chrono::steady_clock::time_point acquired_at;  // When lock was acquired
        
        // Default constructor (required for std::map)
        LockEntry()
            : owner_id(-1),
              acquired_at(std::chrono::steady_clock::now()) {}
        
        // Constructor for creating a lock for a specific client
        LockEntry(int32_t id)
            : owner_id(id),
              acquired_at(std::chrono::steady_clock::now()) {}
    };
    
    mutable std::mutex mutex_;                    // Protects store and locks
    std::map<std::string, ValueEntry> store_;      // Key-value store
    std::map<std::string, LockEntry> locks_;       // Lock table
    int64_t last_timestamp_;                      // Last timestamp generated
    
    // Generate a new monotonically increasing timestamp.
    int64_t GenerateTimestamp();
    
    // Lock timeout: if a lock is held for more than this many seconds,
    // it's considered abandoned and can be overtaken by another client
    static constexpr int LOCK_TIMEOUT_SECONDS = 30;
};

}
