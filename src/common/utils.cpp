#include "utils.h"
#include <sstream>

namespace kvstore {

TimestampGenerator::TimestampGenerator() 
    : last_timestamp_(0)
    , sequence_(0) {
}

int64_t TimestampGenerator::Generate() {
    int64_t current = GetCurrentTimestamp();
    
    // If we've moved to a new millisecond, reset sequence
    if (current > last_timestamp_) {
        last_timestamp_ = current;
        sequence_ = 0;
    } else {
        // Same millisecond - increment sequence to ensure uniqueness
        sequence_++;
    }
    
    // This ensures that even if two operations happen in the same millisecond, they get different timestamps
    return last_timestamp_ * 1000 + sequence_;
}

bool ParseAddress(const std::string& address, std::string& host, int32_t& port) {
    size_t colon_pos = address.find(':');
    if (colon_pos == std::string::npos) {
        return false;  // No colon found - invalid format
    }
    
    host = address.substr(0, colon_pos);
    try {
        port = std::stoi(address.substr(colon_pos + 1));
    } catch (...) {
        return false;  // Port is not a valid integer
    }
    
    return true;
}

std::string FormatAddress(const std::string& host, int32_t port) {
    return host + ":" + std::to_string(port);
}

} // namespace kvstore
