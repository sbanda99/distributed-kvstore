// Performance Evaluation Code

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <mutex>
#include <cstdlib>
#include "../src/client/abd_client.h"
#include "../src/client/blocking_client.h"
#include "../src/common/config.h"

using namespace kvstore;

struct Stats {
    std::atomic<int64_t> total_ops{0};
    std::atomic<int64_t> total_gets{0};
    std::atomic<int64_t> total_puts{0};
    std::atomic<int64_t> failed_ops{0};
    
    // Latency measurements (in microseconds)
    std::vector<int64_t> get_latencies;
    std::vector<int64_t> put_latencies;
    
    std::mutex latency_mutex;
    
    void add_get_latency(int64_t latency_us) {
        std::lock_guard<std::mutex> lock(latency_mutex);
        get_latencies.push_back(latency_us);
    }
    
    void add_put_latency(int64_t latency_us) {
        std::lock_guard<std::mutex> lock(latency_mutex);
        put_latencies.push_back(latency_us);
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

// Global stats
Stats global_stats;

// Worker thread for ABD Protocol
void worker_thread_abd(ABDClient& client, double get_ratio, 
                       int duration_sec, int client_id) {
    auto start_time = std::chrono::steady_clock::now();
    auto end_time = start_time + std::chrono::seconds(duration_sec);
    
    int key_counter = client_id * 10000;
    
    while (std::chrono::steady_clock::now() < end_time) {
        double rand_val = static_cast<double>(rand()) / RAND_MAX;
        bool is_get = (rand_val < get_ratio);
        
        std::string key = "perf_key_" + std::to_string(key_counter);
        auto op_start = std::chrono::high_resolution_clock::now();
        
        if (is_get) {
            std::string value;
            bool success = client.Read(key, value);
            auto op_end = std::chrono::high_resolution_clock::now();
            auto latency_us = std::chrono::duration_cast<std::chrono::microseconds>(
                op_end - op_start).count();
            
            if (success) {
                global_stats.total_gets++;
                global_stats.add_get_latency(latency_us);
            } else {
                global_stats.failed_ops++;
            }
        } else {
            std::string value = "value_" + std::to_string(key_counter);
            bool success = client.Write(key, value);
            auto op_end = std::chrono::high_resolution_clock::now();
            auto latency_us = std::chrono::duration_cast<std::chrono::microseconds>(
                op_end - op_start).count();
            
            if (success) {
                global_stats.total_puts++;
                global_stats.add_put_latency(latency_us);
            } else {
                global_stats.failed_ops++;
            }
        }
        
        global_stats.total_ops++;
        key_counter++;
    }
}

// Worker thread for Blocking Protocol
void worker_thread_blocking(BlockingClient& client, double get_ratio,
                            int duration_sec, int client_id) {
    auto start_time = std::chrono::steady_clock::now();
    auto end_time = start_time + std::chrono::seconds(duration_sec);
    
    int key_counter = client_id * 10000;
    
    while (std::chrono::steady_clock::now() < end_time) {
        double rand_val = static_cast<double>(rand()) / RAND_MAX;
        bool is_get = (rand_val < get_ratio);
        
        std::string key = "perf_key_" + std::to_string(key_counter);
        auto op_start = std::chrono::high_resolution_clock::now();
        
        if (is_get) {
            std::string value;
            bool success = client.Read(key, value);
            auto op_end = std::chrono::high_resolution_clock::now();
            auto latency_us = std::chrono::duration_cast<std::chrono::microseconds>(
                op_end - op_start).count();
            
            if (success) {
                global_stats.total_gets++;
                global_stats.add_get_latency(latency_us);
            } else {
                global_stats.failed_ops++;
            }
        } else {
            std::string value = "value_" + std::to_string(key_counter);
            bool success = client.Write(key, value);
            auto op_end = std::chrono::high_resolution_clock::now();
            auto latency_us = std::chrono::duration_cast<std::chrono::microseconds>(
                op_end - op_start).count();
            
            if (success) {
                global_stats.total_puts++;
                global_stats.add_put_latency(latency_us);
            } else {
                global_stats.failed_ops++;
            }
        }
        
        global_stats.total_ops++;
        key_counter++;
    }
}

void print_results(const std::string& protocol, int num_servers, 
                   int num_clients, double get_ratio, int duration_sec) {
    std::cout << std::endl;
    std::cout << "Performance Evaluation Results" << std::endl;
    std::cout << "Protocol:        " << protocol << std::endl;
    std::cout << "Number of Servers: " << num_servers << std::endl;
    std::cout << "Number of Clients: " << num_clients << std::endl;
    std::cout << "Get Ratio:       " << (get_ratio * 100) << "%" << std::endl;
    std::cout << "Put Ratio:       " << ((1.0 - get_ratio) * 100) << "%" << std::endl;
    std::cout << "Duration:         " << duration_sec << " seconds" << std::endl;
    std::cout << std::endl;
    
    // Throughput
    double throughput = static_cast<double>(global_stats.total_ops) / duration_sec;
    std::cout << "Throughput:" << std::endl;
    std::cout << "  Total Operations: " << global_stats.total_ops << std::endl;
    std::cout << "  Throughput:        " << throughput << " requests/sec" << std::endl;
    std::cout << "  Failed Operations: " << global_stats.failed_ops << std::endl;
    std::cout << std::endl;
    
    // Latency statistics
    int64_t get_median = 0, get_p95 = 0;
    int64_t put_median = 0, put_p95 = 0;
    
    global_stats.compute_percentiles(global_stats.get_latencies, get_median, get_p95);
    global_stats.compute_percentiles(global_stats.put_latencies, put_median, put_p95);
    
    std::cout << "Latency (GET operations):" << std::endl;
    std::cout << "  Total GETs:     " << global_stats.total_gets << std::endl;
    if (!global_stats.get_latencies.empty()) {
        std::cout << "  Median:         " << get_median << " microseconds" << std::endl;
        std::cout << "  95th Percentile: " << get_p95 << " microseconds" << std::endl;
    } else {
        std::cout << "  No GET operations performed" << std::endl;
    }
    std::cout << std::endl;
    
    std::cout << "Latency (PUT operations):" << std::endl;
    std::cout << "  Total PUTs:     " << global_stats.total_puts << std::endl;
    if (!global_stats.put_latencies.empty()) {
        std::cout << "  Median:         " << put_median << " microseconds" << std::endl;
        std::cout << "  95th Percentile: " << put_p95 << " microseconds" << std::endl;
    } else {
        std::cout << "  No PUT operations performed" << std::endl;
    }
}

int main(int argc, char** argv) {
    if (argc < 6) {
        std::cerr << "Usage: " << argv[0] << " <config_file> <protocol> <num_clients> <get_ratio> <duration_sec>" << std::endl;
        return 1;
    }
    
    std::string config_file = argv[1];
    std::string protocol = argv[2];
    int num_clients = std::stoi(argv[3]);
    double get_ratio = std::stod(argv[4]);
    int duration_sec = std::stoi(argv[5]);
    
    if (protocol != "abd" && protocol != "blocking") {
        std::cerr << "Error: Protocol must be 'abd' or 'blocking'" << std::endl;
        return 1;
    }
    
    if (get_ratio < 0.0 || get_ratio > 1.0) {
        std::cerr << "Error: Get ratio must be between 0.0 and 1.0" << std::endl;
        return 1;
    }
    
    Config config;
    if (!config.LoadFromFile(config_file)) {
        std::cerr << "Error: Failed to load config file: " << config_file << std::endl;
        return 1;
    }
    
    int num_servers = static_cast<int>(config.GetServers().size());
    
    // Print startup info to stderr (so it can be filtered out)
    std::cerr << "Starting performance evaluation..." << std::endl;
    std::cerr << "Config: " << config_file << std::endl;
    std::cerr << "Protocol: " << protocol << std::endl;
    std::cerr << "Servers: " << num_servers << std::endl;
    std::cerr << "Clients: " << num_clients << std::endl;
    std::cerr << "Get ratio: " << (get_ratio * 100) << "%" << std::endl;
    std::cerr << "Duration: " << duration_sec << " seconds" << std::endl;
    std::cerr << std::endl;
    
    // Print server addresses to stderr
    std::cerr << "Server addresses:" << std::endl;
    for (const auto& server : config.GetServers()) {
        std::cerr << "  Server " << server.id << ": " << server.GetAddress() << std::endl;
    }
    std::cerr << std::endl;
    
    std::cerr << "Starting test..." << std::endl;
    
    global_stats.total_ops = 0;
    global_stats.total_gets = 0;
    global_stats.total_puts = 0;
    global_stats.failed_ops = 0;
    global_stats.get_latencies.clear();
    global_stats.put_latencies.clear();
    
    // Start worker threads
    std::vector<std::thread> threads;
    auto test_start = std::chrono::steady_clock::now();
    
    if (protocol == "abd") {
        for (int i = 0; i < num_clients; i++) {
            ABDClient* client = new ABDClient(config);
            threads.emplace_back(worker_thread_abd, std::ref(*client), 
                                get_ratio, duration_sec, i);
        }
    } else {
        for (int i = 0; i < num_clients; i++) {
            BlockingClient* client = new BlockingClient(config, i + 1);
            threads.emplace_back(worker_thread_blocking, std::ref(*client),
                                get_ratio, duration_sec, i);
        }
    }
    
    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }
    
    auto test_end = std::chrono::steady_clock::now();
    auto actual_duration = std::chrono::duration_cast<std::chrono::seconds>(
        test_end - test_start).count();
    
    // Print results
    print_results(protocol, num_servers, num_clients, get_ratio, actual_duration);
    
    return 0;
}

