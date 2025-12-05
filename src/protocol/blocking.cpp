// This file implements the Blocking Protocol Implementation
// Server-side storage and lock management for the blocking protocol.

#include "blocking.h"
#include "../common/utils.h"
#include <algorithm>
#include <chrono>

namespace kvstore {

BlockingProtocol::BlockingProtocol() : last_timestamp_(0) {
}

BlockingProtocol::~BlockingProtocol() {
}

BlockingProtocol::LockResult BlockingProtocol::AcquireLock(const std::string& key, 
                                                           int32_t client_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    LockResult result;
    result.granted = false;
    result.timestamp = kvstore::GetCurrentTimestamp();
    
    // Check if the key is already locked
    auto lock_it = locks_.find(key);
    if (lock_it != locks_.end()) {
        // Key is locked - check if we should grant the lock anyway
        
        // Check if the lock has timed out (client may have crashed)
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - lock_it->second.acquired_at).count();
        
        if (elapsed > LOCK_TIMEOUT_SECONDS) {
            // Lock timed out - assume previous client crashed, steal the lock
            locks_.erase(lock_it);
            locks_[key] = LockEntry(client_id);
            result.granted = true;
            result.timestamp = kvstore::GetCurrentTimestamp();
        } else if (lock_it->second.owner_id == client_id) {
            // Same client already has the lock (re-entrant lock)
            result.granted = true;
            result.timestamp = kvstore::GetCurrentTimestamp();
        } else {
            // Lock is held by another client and hasn't timed out
            // This is the "blocking" behavior - client must wait
            result.granted = false;
            result.timestamp = kvstore::GetCurrentTimestamp();
        }
    } else {
        // Key is not locked - grant the lock to this client
        locks_[key] = LockEntry(client_id);
        result.granted = true;
        result.timestamp = kvstore::GetCurrentTimestamp();
    }
    
    return result;
}

bool BlockingProtocol::ReleaseLock(const std::string& key, int32_t client_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto lock_it = locks_.find(key);
    // Only release if this client actually holds the lock
    if (lock_it != locks_.end() && lock_it->second.owner_id == client_id) {
        locks_.erase(lock_it);
        return true;
    }
    
    return false;
}

BlockingProtocol::ReadResult BlockingProtocol::Read(const std::string& key, 
                                                    int32_t client_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    ReadResult result;
    result.success = false;
    result.timestamp = 0;
    
    // Verify that this client holds the lock
    auto lock_it = locks_.find(key);
    if (lock_it == locks_.end() || lock_it->second.owner_id != client_id) {
        // Client doesn't have the lock - reject the read
        result.success = false;
        return result;
    }
    
    // Client has the lock - perform the read
    auto it = store_.find(key);
    if (it != store_.end()) {
        result.value = it->second.value;
        result.timestamp = it->second.timestamp;
        result.success = true;
    } else {
        // Key doesn't exist
        result.value = "";
        result.timestamp = 0;
        result.success = true;
    }
    
    return result;
}

BlockingProtocol::WriteResult BlockingProtocol::Write(const std::string& key, 
                                                     const std::string& value, 
                                                     int64_t client_timestamp, 
                                                     int32_t client_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    WriteResult result;
    result.success = false;
    
    // Verify that this client holds the lock
    auto lock_it = locks_.find(key);
    if (lock_it == locks_.end() || lock_it->second.owner_id != client_id) {
        // Client doesn't have the lock - reject the write
        result.success = false;
        return result;
    }
    
    // Client has the lock - perform the write
    int64_t server_timestamp = GenerateTimestamp();
    
    // Use maximum of client and server timestamps (same as ABD)
    int64_t final_timestamp = std::max(client_timestamp, server_timestamp);
    
    store_[key] = ValueEntry(value, final_timestamp);
    result.success = true;
    result.timestamp = final_timestamp;
    
    return result;
}

int64_t BlockingProtocol::GetTimestamp(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = store_.find(key);
    if (it != store_.end()) {
        return it->second.timestamp;
    }
    return 0;
}

std::string BlockingProtocol::GetValue(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = store_.find(key);
    if (it != store_.end()) {
        return it->second.value;
    }
    return "";
}

bool BlockingProtocol::IsLocked(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return locks_.find(key) != locks_.end();
}

int32_t BlockingProtocol::GetLockOwner(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto lock_it = locks_.find(key);
    if (lock_it != locks_.end()) {
        return lock_it->second.owner_id;
    }
    return -1;  // No lock held
}

int64_t BlockingProtocol::GenerateTimestamp() {
    // Generate monotonically increasing timestamp (same logic as ABD)
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    int64_t current = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    
    if (current > last_timestamp_) {
        last_timestamp_ = current;
    } else {
        last_timestamp_++;
    }
    
    return last_timestamp_;
}

}
