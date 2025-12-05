// Client Crash Impact Evaluation Code

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <algorithm>
#include <mutex>
#include <cstdlib>
#include "../src/client/abd_client.h"
#include "../src/client/blocking_client.h"
#include "../src/common/config.h"

using namespace kvstore;

struct CrashStats {
    std::atomic<int64_t> ops_before_crash{0};
    std::atomic<int64_t> ops_after_crash{0};
    std::atomic<int64_t> failed_ops_before{0};
    std::atomic<int64_t> failed_ops_after{0};
    
    std::vector<int64_t> latencies_before;
    std::vector<int64_t> latencies_after;
    
    std::mutex latency_mutex;
    
    void add_latency_before(int64_t latency_us) {
        std::lock_guard<std::mutex> lock(latency_mutex);
        latencies_before.push_back(latency_us);
    }
    
    void add_latency_after(int64_t latency_us) {
        std::lock_guard<std::mutex> lock(latency_mutex);
        latencies_after.push_back(latency_us);
    }
    
    void compute_percentiles(const std::vector<int64_t>& latencies,
                             int64_t& median, int64_t& p95) {
        if (latencies.empty()) {
            median = 0;
            p95 = 0;
            return;
        }
        
        std::vector<int64_t> sorted = latencies;
        std::sort(sorted.begin(), sorted.end());
        
        median = sorted[sorted.size() / 2];
        p95 = sorted[static_cast<size_t>(sorted.size() * 0.95)];
    }
};

CrashStats global_crash_stats;
std::atomic<bool> crash_occurred{false};

void worker_thread_abd(ABDClient& client, int client_id, 
                       int crash_after_sec, int total_duration_sec,
                       bool is_crash_client) {
    auto start_time = std::chrono::steady_clock::now();
    auto crash_time = start_time + std::chrono::seconds(crash_after_sec);
    auto end_time = start_time + std::chrono::seconds(total_duration_sec);
    
    int key_counter = client_id * 10000;
    
    while (std::chrono::steady_clock::now() < end_time) {
        // Simulate crash: stop this client after crash_time
        if (is_crash_client && std::chrono::steady_clock::now() >= crash_time) {
            if (!crash_occurred.load()) {
                crash_occurred.store(true);
                std::cout << "[Client " << client_id << "] CRASHED at " 
                          << crash_after_sec << " seconds" << std::endl;
            }
            break;  // Client crashes - stop processing
        }
        
        bool is_before_crash = !crash_occurred.load();
        
        std::string key = "crash_test_key_" + std::to_string(key_counter);
        auto op_start = std::chrono::high_resolution_clock::now();
        
        // Alternate between read and write
        if (key_counter % 2 == 0) {
            std::string value;
            bool success = client.Read(key, value);
            auto op_end = std::chrono::high_resolution_clock::now();
            auto latency_us = std::chrono::duration_cast<std::chrono::microseconds>(
                op_end - op_start).count();
            
            if (success) {
                if (is_before_crash) {
                    global_crash_stats.ops_before_crash++;
                    global_crash_stats.add_latency_before(latency_us);
                } else {
                    global_crash_stats.ops_after_crash++;
                    global_crash_stats.add_latency_after(latency_us);
                }
            } else {
                if (is_before_crash) {
                    global_crash_stats.failed_ops_before++;
                } else {
                    global_crash_stats.failed_ops_after++;
                }
            }
        } else {
            std::string value = "value_" + std::to_string(key_counter);
            bool success = client.Write(key, value);
            auto op_end = std::chrono::high_resolution_clock::now();
            auto latency_us = std::chrono::duration_cast<std::chrono::microseconds>(
                op_end - op_start).count();
            
            if (success) {
                if (is_before_crash) {
                    global_crash_stats.ops_before_crash++;
                    global_crash_stats.add_latency_before(latency_us);
                } else {
                    global_crash_stats.ops_after_crash++;
                    global_crash_stats.add_latency_after(latency_us);
                }
            } else {
                if (is_before_crash) {
                    global_crash_stats.failed_ops_before++;
                } else {
                    global_crash_stats.failed_ops_after++;
                }
            }
        }
        
        key_counter++;
        
    }
}

void worker_thread_blocking(BlockingClient& client, int client_id,
                            int crash_after_sec, int total_duration_sec,
                            bool is_crash_client) {
    auto start_time = std::chrono::steady_clock::now();
    auto crash_time = start_time + std::chrono::seconds(crash_after_sec);
    auto end_time = start_time + std::chrono::seconds(total_duration_sec);
    
    int key_counter = client_id * 10000;
    
    while (std::chrono::steady_clock::now() < end_time) {
        if (is_crash_client && std::chrono::steady_clock::now() >= crash_time) {
            if (!crash_occurred.load()) {
                crash_occurred.store(true);
                std::cout << "[Client " << client_id << "] CRASHED at " 
                          << crash_after_sec << " seconds" << std::endl;
            }
            break;
        }
        
        bool is_before_crash = !crash_occurred.load();
        
        std::string key = "crash_test_key_" + std::to_string(key_counter);
        auto op_start = std::chrono::high_resolution_clock::now();
        
        if (key_counter % 2 == 0) {
            std::string value;
            bool success = client.Read(key, value);
            auto op_end = std::chrono::high_resolution_clock::now();
            auto latency_us = std::chrono::duration_cast<std::chrono::microseconds>(
                op_end - op_start).count();
            
            if (success) {
                if (is_before_crash) {
                    global_crash_stats.ops_before_crash++;
                    global_crash_stats.add_latency_before(latency_us);
                } else {
                    global_crash_stats.ops_after_crash++;
                    global_crash_stats.add_latency_after(latency_us);
                }
            } else {
                if (is_before_crash) {
                    global_crash_stats.failed_ops_before++;
                } else {
                    global_crash_stats.failed_ops_after++;
                }
            }
        } else {
            std::string value = "value_" + std::to_string(key_counter);
            bool success = client.Write(key, value);
            auto op_end = std::chrono::high_resolution_clock::now();
            auto latency_us = std::chrono::duration_cast<std::chrono::microseconds>(
                op_end - op_start).count();
            
            if (success) {
                if (is_before_crash) {
                    global_crash_stats.ops_before_crash++;
                    global_crash_stats.add_latency_before(latency_us);
                } else {
                    global_crash_stats.ops_after_crash++;
                    global_crash_stats.add_latency_after(latency_us);
                }
            } else {
                if (is_before_crash) {
                    global_crash_stats.failed_ops_before++;
                } else {
                    global_crash_stats.failed_ops_after++;
                }
            }
        }
        
        key_counter++;
    }
}

void print_crash_results(const std::string& protocol, int num_servers,
                         int num_clients, int crash_after_sec, int total_duration_sec) {
    std::cout << std::endl;
    std::cout << "Client Crash Impact Evaluation Results" << std::endl;
    std::cout << "Protocol:        " << protocol << std::endl;
    std::cout << "Number of Servers: " << num_servers << std::endl;
    std::cout << "Number of Clients: " << num_clients << std::endl;
    std::cout << "Crash Time:       " << crash_after_sec << " seconds" << std::endl;
    std::cout << "Total Duration:   " << total_duration_sec << " seconds" << std::endl;
    std::cout << std::endl;
    
    int before_duration = crash_after_sec;
    int after_duration = total_duration_sec - crash_after_sec;
    
    double throughput_before = static_cast<double>(global_crash_stats.ops_before_crash) / before_duration;
    double throughput_after = static_cast<double>(global_crash_stats.ops_after_crash) / after_duration;
    
    std::cout << "BEFORE Crash (0-" << crash_after_sec << " seconds):" << std::endl;
    std::cout << "  Operations:     " << global_crash_stats.ops_before_crash << std::endl;
    std::cout << "  Throughput:     " << throughput_before << " ops/sec" << std::endl;
    std::cout << "  Failed Ops:     " << global_crash_stats.failed_ops_before << std::endl;
    
    int64_t median_before, p95_before;
    global_crash_stats.compute_percentiles(global_crash_stats.latencies_before, 
                                          median_before, p95_before);
    std::cout << "  Median Latency: " << median_before << " microseconds" << std::endl;
    std::cout << "  95th Percentile: " << p95_before << " microseconds" << std::endl;
    std::cout << std::endl;
    
    std::cout << "AFTER Crash (" << crash_after_sec << "-" << total_duration_sec << " seconds):" << std::endl;
    std::cout << "  Operations:     " << global_crash_stats.ops_after_crash << std::endl;
    std::cout << "  Throughput:     " << throughput_after << " ops/sec" << std::endl;
    std::cout << "  Failed Ops:     " << global_crash_stats.failed_ops_after << std::endl;
    
    int64_t median_after, p95_after;
    global_crash_stats.compute_percentiles(global_crash_stats.latencies_after,
                                          median_after, p95_after);
    std::cout << "  Median Latency: " << median_after << " microseconds" << std::endl;
    std::cout << "  95th Percentile: " << p95_after << " microseconds" << std::endl;
    std::cout << std::endl;
    
    double throughput_change = ((throughput_after - throughput_before) / throughput_before) * 100.0;
    double latency_change = ((median_after - median_before) / static_cast<double>(median_before)) * 100.0;
    
    std::cout << "Impact:" << std::endl;
    std::cout << "  Throughput Change: " << throughput_change << "%" << std::endl;
    std::cout << "  Latency Change:    " << latency_change << "%" << std::endl;
}

int main(int argc, char** argv) {
    if (argc < 6) {
        std::cerr << "Usage: " << argv[0] << " <config_file> <protocol> <num_clients> <crash_after_sec> <total_duration_sec>" << std::endl;
        return 1;
    }
    
    std::string config_file = argv[1];
    std::string protocol = argv[2];
    int num_clients = std::stoi(argv[3]);
    int crash_after_sec = std::stoi(argv[4]);
    int total_duration_sec = std::stoi(argv[5]);
    
    if (protocol != "abd" && protocol != "blocking") {
        std::cerr << "Error: Protocol must be 'abd' or 'blocking'" << std::endl;
        return 1;
    }
    
    if (num_clients < 2) {
        std::cerr << "Error: Need at least 2 clients (1 to crash, 1+ to remain)" << std::endl;
        return 1;
    }
    
    Config config;
    if (!config.LoadFromFile(config_file)) {
        std::cerr << "Error: Failed to load config file: " << config_file << std::endl;
        return 1;
    }
    
    int num_servers = static_cast<int>(config.GetServers().size());
    
    std::cout << "Starting crash impact evaluation..." << std::endl;
    std::cout << "Config: " << config_file << std::endl;
    std::cout << "Protocol: " << protocol << std::endl;
    std::cout << "Servers: " << num_servers << std::endl;
    std::cout << "Clients: " << num_clients << " (1 will crash after " << crash_after_sec << " seconds)" << std::endl;
    std::cout << "Duration: " << total_duration_sec << " seconds" << std::endl;
    std::cout << std::endl;
    
    // Print server addresses from config
    std::cout << "Server addresses:" << std::endl;
    for (const auto& server : config.GetServers()) {
        std::cout << "  Server " << server.id << ": " << server.GetAddress() << std::endl;
    }
    std::cout << std::endl;
    
    global_crash_stats.ops_before_crash = 0;
    global_crash_stats.ops_after_crash = 0;
    global_crash_stats.failed_ops_before = 0;
    global_crash_stats.failed_ops_after = 0;
    global_crash_stats.latencies_before.clear();
    global_crash_stats.latencies_after.clear();
    crash_occurred.store(false);
    
    // Start worker threads
    std::vector<std::thread> threads;
    
    if (protocol == "abd") {
        for (int i = 0; i < num_clients; i++) {
            ABDClient* client = new ABDClient(config);
            bool is_crash_client = (i == 0);  // First client crashes
            threads.emplace_back(worker_thread_abd, std::ref(*client), i,
                                crash_after_sec, total_duration_sec, is_crash_client);
        }
    } else {
        for (int i = 0; i < num_clients; i++) {
            BlockingClient* client = new BlockingClient(config, i + 1);
            bool is_crash_client = (i == 0);
            threads.emplace_back(worker_thread_blocking, std::ref(*client), i,
                                crash_after_sec, total_duration_sec, is_crash_client);
        }
    }
    
    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }
    
    // Print results
    print_crash_results(protocol, num_servers, num_clients, crash_after_sec, total_duration_sec);
    
    return 0;
}

