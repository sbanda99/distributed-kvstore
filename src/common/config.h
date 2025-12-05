
// This module handles loading and parsing of JSON configuration files.
// Configuration files specify server addresses, protocol type, and quorum sizes.

#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace kvstore {

// Represents information about a single server in the cluster.
// Each server has a unique ID, hostname/IP, and port number.
struct ServerInfo {
    int32_t id;           // Unique server identifier
    std::string host;     // Hostname or IP address
    int32_t port;         // Port number
    
    // Returns the server address in "host:port" format.
    // This is used to create gRPC connection strings.
    std::string GetAddress() const {
        return host + ":" + std::to_string(port);
    }
};

// Protocol type enumeration.
// ABD: Non-blocking, wait-free protocol
// BLOCKING: Lock-based protocol that may block on client failures
enum class ProtocolType {
    ABD,
    BLOCKING
};

// Configuration manager for the key-value store.
// Loads configuration from JSON files and provides access to:
// - Server list and addresses
// - Protocol type (ABD or Blocking)
// - Quorum sizes (read quorum R, write quorum W)
// - Number of replicas
class Config {
public:
    Config();
    ~Config();
    
    // Load configuration from a JSON file.
    // @param filename Path to the JSON configuration file
    // @return true if configuration was loaded successfully, false otherwise
    bool LoadFromFile(const std::string& filename);
    
    // Getters for configuration values
    const std::vector<ServerInfo>& GetServers() const { return servers_; }
    ServerInfo GetServer(int32_t id) const;
    ProtocolType GetProtocol() const { return protocol_; }
    int32_t GetReadQuorum() const { return read_quorum_; }
    int32_t GetWriteQuorum() const { return write_quorum_; }
    int32_t GetNumReplicas() const { return num_replicas_; }
    int32_t GetServerId() const { return server_id_; }
    int32_t GetPort() const { return port_; }
    
    // Setters (mainly for testing or programmatic configuration)
    void SetServers(const std::vector<ServerInfo>& servers) { servers_ = servers; }
    void SetProtocol(ProtocolType protocol) { protocol_ = protocol; }
    void SetReadQuorum(int32_t r) { read_quorum_ = r; }
    void SetWriteQuorum(int32_t w) { write_quorum_ = w; }
    void SetNumReplicas(int32_t n) { num_replicas_ = n; }
    void SetServerId(int32_t id) { server_id_ = id; }
    void SetPort(int32_t port) { port_ = port; }
    
    // Validate the configuration.
    // Checks that servers are configured, quorums are valid, etc.
    // @return true if configuration is valid, false otherwise
    bool Validate() const;

private:
    std::vector<ServerInfo> servers_;  // List of all servers in the cluster
    ProtocolType protocol_;            // Which protocol to use (ABD or Blocking)
    int32_t read_quorum_;              // Number of servers needed for a read (R)
    int32_t write_quorum_;             // Number of servers needed for a write (W)
    int32_t num_replicas_;            // Total number of replicas
    int32_t server_id_;                // This server's ID (if running as server)
    int32_t port_;                    // Port to listen on (if running as server)
};

} // namespace kvstore
