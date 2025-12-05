// Correctness Tests for Blocking Protocol

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include "../src/client/blocking_client.h"
#include "../src/common/config.h"

using namespace kvstore;

int tests_passed = 0;
int tests_failed = 0;

void assert_test(bool condition, const std::string& test_name) {
    if (condition) {
        std::cout << "✓ PASS: " << test_name << std::endl;
        tests_passed++;
    } else {
        std::cerr << "✗ FAIL: " << test_name << std::endl;
        tests_failed++;
    }
}

// Test 1: Basic Write and Read
void test_basic_write_read(BlockingClient& client) {
    std::string key = "test_key_1";
    std::string value = "test_value_1";
    std::string read_value;
    
    bool write_ok = client.Write(key, value);
    assert_test(write_ok, "Basic write operation");
    
    bool read_ok = client.Read(key, read_value);
    assert_test(read_ok, "Basic read operation");
    assert_test(read_value == value, "Read returns correct value");
}

// Test 2: Write Overwrite
void test_write_overwrite(BlockingClient& client) {
    std::string key = "test_key_2";
    std::string value1 = "value1";
    std::string value2 = "value2";
    std::string read_value;
    
    client.Write(key, value1);
    client.Write(key, value2);
    client.Read(key, read_value);
    
    assert_test(read_value == value2, "Write overwrites previous value");
}

// Test 3: Multiple Keys
void test_multiple_keys(BlockingClient& client) {
    std::string key1 = "key1";
    std::string key2 = "key2";
    std::string value1 = "value1";
    std::string value2 = "value2";
    std::string read1, read2;
    
    client.Write(key1, value1);
    client.Write(key2, value2);
    
    client.Read(key1, read1);
    client.Read(key2, read2);
    
    assert_test(read1 == value1, "Key1 has correct value");
    assert_test(read2 == value2, "Key2 has correct value");
}

// Test 4: Lock Exclusion
void test_lock_exclusion(BlockingClient& client1, BlockingClient& client2) {
    std::string key = "lock_test_key";
    std::string value1 = "client1_value";
    std::string value2 = "client2_value";
    
    // Client 1 writes
    bool write1_ok = client1.Write(key, value1);
    assert_test(write1_ok, "Client1 write succeeds");
    
    // Small delay
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Client 2 writes (should succeed, locks are released after each operation)
    bool write2_ok = client2.Write(key, value2);
    assert_test(write2_ok, "Client2 write succeeds (locks released)");
    
    // Both should read the same value (last write wins)
    std::string read1, read2;
    client1.Read(key, read1);
    client2.Read(key, read2);
    
    assert_test(read1 == read2, "Both clients see the same value (consistency)");
    assert_test(read1 == value2, "Last write is visible to all clients");
}

// Test 5: Sequential Operations
void test_sequential_operations(BlockingClient& client) {
    std::string key = "sequential_key";
    std::vector<std::string> values = {"v1", "v2", "v3", "v4", "v5"};
    
    for (const auto& value : values) {
        client.Write(key, value);
    }
    
    std::string read_value;
    client.Read(key, read_value);
    assert_test(read_value == values.back(), "Sequential writes maintain consistency");
}

// Test 6: Read After Write Consistency
void test_read_after_write(BlockingClient& client1, BlockingClient& client2) {
    std::string key = "consistency_key";
    std::string value = "consistent_value";
    
    bool write_ok = client1.Write(key, value);
    assert_test(write_ok, "Client1 write succeeds");
    
    // Small delay
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::string read1, read2;
    client1.Read(key, read1);
    client2.Read(key, read2);
    
    assert_test(read1 == value, "Client1 reads its own write");
    assert_test(read2 == value, "Client2 reads Client1's write (consistency)");
}

// Test 7: Concurrent Operations
void test_concurrent_different_keys(BlockingClient& client1, BlockingClient& client2, BlockingClient& client3) {
    std::string key1 = "concurrent_key1";
    std::string key2 = "concurrent_key2";
    std::string key3 = "concurrent_key3";
    std::string value1 = "value1";
    std::string value2 = "value2";
    std::string value3 = "value3";
    
    // Write concurrently to different keys
    std::thread t1([&]() { client1.Write(key1, value1); });
    std::thread t2([&]() { client2.Write(key2, value2); });
    std::thread t3([&]() { client3.Write(key3, value3); });
    
    t1.join();
    t2.join();
    t3.join();
    
    // Verify all writes succeeded
    std::string read1, read2, read3;
    client1.Read(key1, read1);
    client2.Read(key2, read2);
    client3.Read(key3, read3);
    
    assert_test(read1 == value1, "Client1's write to key1 succeeded");
    assert_test(read2 == value2, "Client2's write to key2 succeeded");
    assert_test(read3 == value3, "Client3's write to key3 succeeded");
}

// Test 8: Empty Value
void test_empty_value(BlockingClient& client) {
    std::string key = "empty_key";
    std::string empty_value = "";
    std::string read_value;
    
    client.Write(key, empty_value);
    client.Read(key, read_value);
    
    assert_test(read_value == empty_value, "Empty value can be stored and retrieved");
}

// Test 9: Non-existent Key
void test_nonexistent_key(BlockingClient& client) {
    std::string key = "nonexistent_key_12345";
    std::string read_value;
    
    bool read_ok = client.Read(key, read_value);
    assert_test(read_ok, "Read of non-existent key succeeds");
    assert_test(read_value == "", "Read of non-existent key returns empty string");
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        std::cerr << "Example: " << argv[0] << " ../config/config_3servers_blocking.json" << std::endl;
        return 1;
    }
    
    std::string config_file = argv[1];
    Config config;
    
    if (!config.LoadFromFile(config_file)) {
        std::cerr << "Error: Failed to load config file: " << config_file << std::endl;
        return 1;
    }
    
    std::cout << "Blocking Protocol Correctness Tests" << std::endl;
    std::cout << "Config: " << config_file << std::endl;
    std::cout << "Servers: " << config.GetServers().size() << std::endl;
    
    // Create clients with different IDs
    BlockingClient client1(config, 1);
    BlockingClient client2(config, 2);
    BlockingClient client3(config, 3);
    
    // Run tests
    std::cout << "Running Blocking Protocol correctness tests..." << std::endl;
    std::cout << std::endl;
    
    test_basic_write_read(client1);
    test_write_overwrite(client1);
    test_multiple_keys(client1);
    test_empty_value(client1);
    test_nonexistent_key(client1);
    test_sequential_operations(client1);
    test_read_after_write(client1, client2);
    test_lock_exclusion(client1, client2);
    test_concurrent_different_keys(client1, client2, client3);
    
    // Print summary
    std::cout << std::endl;
    std::cout << "Test Summary" << std::endl;
    std::cout << "  Passed: " << tests_passed << std::endl;
    std::cout << "  Failed: " << tests_failed << std::endl;
    std::cout << "  Total:  " << (tests_passed + tests_failed) << std::endl;
    
    return (tests_failed == 0) ? 0 : 1;
}

