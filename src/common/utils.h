// Provides helper functions for timestamps and address formatting.
// Timestamps are used to ensure linearizability in both protocols.

#pragma once

#include <string>
#include <chrono>
#include <cstdint>

namespace kvstore {

// Get the current system time as milliseconds since epoch.
// Used for generating timestamps for key-value operations.
inline int64_t GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

// Timestamp generator that ensures unique, monotonically increasing timestamps.
// If multiple operations happen in the same millisecond, this adds a sequence
// number to ensure uniqueness. This is needed for maintaining ordering
class TimestampGenerator {
public:
    TimestampGenerator();
    
    // Generate a unique timestamp.
    // Combines current time with a sequence number for uniqueness.
    // @return Unique timestamp value
    int64_t Generate();
    
private:
    int64_t last_timestamp_;  // Last timestamp we generated
    int32_t sequence_;        // Sequence number for same-millisecond events
};

// Parse a server address string in "host:port" format.
// @param address Address string to parse
// @param host Output parameter for hostname/IP
// @param port Output parameter for port number
// @return true if parsing succeeded, false otherwise
bool ParseAddress(const std::string& address, std::string& host, int32_t& port);

// Format a host and port into "host:port" string.
// @param host Hostname or IP address
// @param port Port number
// @return Formatted address string in "host:port" format
std::string FormatAddress(const std::string& host, int32_t port);

} // namespace kvstore
