// Blocking Client Main Program

#include <iostream>
#include <string>
#include "blocking_client.h"
#include "../common/config.h"

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <config_file> <client_id> [commands...]" << std::endl;
        std::cerr << "Commands:" << std::endl;
        std::cerr << "  read <key>" << std::endl;
        std::cerr << "  write <key> <value>" << std::endl;
        return 1;
    }
    
    std::string config_file = argv[1];
    int32_t client_id = std::stoi(argv[2]);
    
    kvstore::Config config;
    if (!config.LoadFromFile(config_file)) {
        std::cerr << "Error: Failed to load config file: " << config_file << std::endl;
        return 1;
    }
    
    kvstore::BlockingClient client(config, client_id);
    
    // Interactive mode if no commands provided
    if (argc == 3) {
        std::cout << "Blocking Client (ID: " << client_id << ") - Interactive Mode" << std::endl;
        std::cout << "Commands: read <key>, write <key> <value>, quit" << std::endl;
        
        std::string command;
        while (std::cout << "> " && std::getline(std::cin, command)) {
            if (command == "quit" || command == "exit") {
                break;
            }
            
            if (command.substr(0, 4) == "read") {
                std::string key = command.substr(5);
                std::string value;
                if (client.Read(key, value)) {
                    std::cout << "Value: " << value << std::endl;
                } else {
                    std::cerr << "Error: Read failed" << std::endl;
                }
            } else if (command.substr(0, 5) == "write") {
                size_t space1 = command.find(' ', 6);
                if (space1 != std::string::npos) {
                    std::string key = command.substr(6, space1 - 6);
                    std::string value = command.substr(space1 + 1);
                    if (client.Write(key, value)) {
                        std::cout << "Write successful" << std::endl;
                    } else {
                        std::cerr << "Error: Write failed" << std::endl;
                    }
                } else {
                    std::cerr << "Usage: write <key> <value>" << std::endl;
                }
            }
        }
    } else {
        // Command-line mode: parse commands from arguments
        for (int i = 3; i < argc; i++) {
            std::string cmd = argv[i];
            if (cmd == "read" && i + 1 < argc) {
                std::string key = argv[++i];
                std::string value;
                if (client.Read(key, value)) {
                    std::cout << value << std::endl;
                } else {
                    std::cerr << "Error: Read failed" << std::endl;
                    return 1;
                }
            } else if (cmd == "write" && i + 2 < argc) {
                std::string key = argv[++i];
                std::string value = argv[++i];
                if (!client.Write(key, value)) {
                    std::cerr << "Error: Write failed" << std::endl;
                    return 1;
                }
            }
        }
    }
    
    return 0;
}
