// Parses JSON configuration files using a simple string-based parser.

#include "config.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cctype>

namespace kvstore {

Config::Config() 
    : protocol_(ProtocolType::ABD)
    , read_quorum_(0)
    , write_quorum_(0)
    , num_replicas_(0)
    , server_id_(0)
    , port_(0) {
}

Config::~Config() {
}

bool Config::LoadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open config file: " << filename << std::endl;
        return false;
    }
    
    // Read entire file into a string
    std::string line;
    std::string content;
    while (std::getline(file, line)) {
        content += line;
    }
    file.close();
    
    // Remove all whitespace to simplify parsing
    content.erase(std::remove_if(content.begin(), content.end(), 
        [](unsigned char c) { return std::isspace(c); }), content.end());
    
    // Parse servers array
    // Expected format: "servers":[{"id":0,"host":"localhost","port":5001},...]
    size_t servers_pos = content.find("\"servers\"");
    if (servers_pos != std::string::npos) {
        size_t array_start = content.find('[', servers_pos);
        size_t array_end = content.find(']', array_start);
        
        // Parse each server entry in the array
        size_t pos = array_start;
        while (pos < array_end) {
            // Find server ID
            size_t id_pos = content.find("\"id\"", pos);
            if (id_pos == std::string::npos || id_pos > array_end) break;
            
            size_t id_val_start = content.find(':', id_pos) + 1;
            size_t id_val_end = content.find_first_of(",}", id_val_start);
            int32_t id = std::stoi(content.substr(id_val_start, id_val_end - id_val_start));
            
            // Find host (string value)
            size_t host_pos = content.find("\"host\"", id_pos);
            if (host_pos == std::string::npos) {
                break;
            }
            // Pattern after whitespace removal: "host":"localhost"
            size_t host_colon_pos = content.find(':', host_pos);
            size_t host_val_start = content.find('"', host_colon_pos + 1);
            if (host_val_start == std::string::npos) {
                break;
            }
            host_val_start += 1; // Move past opening quote
            size_t host_val_end = content.find('"', host_val_start);
            if (host_val_end == std::string::npos) {
                break;
            }
            std::string host = content.substr(host_val_start, host_val_end - host_val_start);
            
            // Find port (integer value)
            size_t port_pos = content.find("\"port\"", host_pos);
            size_t port_val_start = content.find(':', port_pos) + 1;
            size_t port_val_end = content.find_first_of(",}", port_val_start);
            int32_t port = std::stoi(content.substr(port_val_start, port_val_end - port_val_start));
            
            servers_.push_back({id, host, port});
            pos = port_val_end;
        }
    }
    
    // Parse protocol type
    // Expected format: "protocol":"abd" or "protocol":"blocking"
    size_t protocol_pos = content.find("\"protocol\"");
    if (protocol_pos != std::string::npos) {
        size_t val_start = content.find('"', protocol_pos) + 1;
        size_t val_end = content.find('"', val_start);
        std::string protocol_str = content.substr(val_start, val_end - val_start);
        if (protocol_str == "abd") {
            protocol_ = ProtocolType::ABD;
        } else if (protocol_str == "blocking") {
            protocol_ = ProtocolType::BLOCKING;
        }
    }
    
    // Parse read quorum
    size_t read_q_pos = content.find("\"read_quorum\"");
    if (read_q_pos != std::string::npos) {
        size_t val_start = content.find(':', read_q_pos) + 1;
        size_t val_end = content.find_first_of(",}", val_start);
        read_quorum_ = std::stoi(content.substr(val_start, val_end - val_start));
    }
    
    // Parse write quorum
    size_t write_q_pos = content.find("\"write_quorum\"");
    if (write_q_pos != std::string::npos) {
        size_t val_start = content.find(':', write_q_pos) + 1;
        size_t val_end = content.find_first_of(",}", val_start);
        write_quorum_ = std::stoi(content.substr(val_start, val_end - val_start));
    }
    
    // Parse number of replicas
    size_t num_rep_pos = content.find("\"num_replicas\"");
    if (num_rep_pos != std::string::npos) {
        size_t val_start = content.find(':', num_rep_pos) + 1;
        size_t val_end = content.find_first_of(",}", val_start);
        num_replicas_ = std::stoi(content.substr(val_start, val_end - val_start));
    }
    
    return Validate();
}

ServerInfo Config::GetServer(int32_t id) const {
    for (const auto& server : servers_) {
        if (server.id == id) {
            return server;
        }
    }
    return ServerInfo{0, "", 0};  // Return invalid server if not found
}

bool Config::Validate() const {
    // Must have at least one server configured
    if (servers_.empty()) {
        std::cerr << "Error: No servers configured" << std::endl;
        return false;
    }
    
    // Quorums must be positive
    if (read_quorum_ <= 0 || write_quorum_ <= 0) {
        std::cerr << "Error: Invalid quorum sizes" << std::endl;
        return false;
    }
    
    // Warn if num_replicas doesn't match actual server count
    if (num_replicas_ > 0 && static_cast<size_t>(num_replicas_) != servers_.size()) {
        std::cerr << "Warning: num_replicas doesn't match number of servers" << std::endl;
    }
    
    // Warn if quorum sizes don't guarantee consistency
    // For linearizability, we need R + W > N (where N is number of servers)
    int32_t n = static_cast<int32_t>(servers_.size());
    if (read_quorum_ + write_quorum_ <= n) {
        std::cerr << "Warning: Quorum sizes may not guarantee consistency" << std::endl;
    }
    
    return true;
}

} // namespace kvstore
