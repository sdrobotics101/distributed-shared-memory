#include <iostream>
#include <sstream>
#include <vector>
#include <atomic>
#include <csignal>

#include <boost/program_options.hpp>

#include "../src/Client/DSMClient.h"

namespace po = boost::program_options;

std::atomic<bool> isRunning;

void signalHandler(int signum) {
    isRunning = false;
}

void processInput(dsm::Client* client) {
    std::cout << "> ";

    std::string input;
    std::getline(std::cin, input);
    std::stringstream ss(input);
    std::string token;
    std::vector<std::string> tokens;
    while (std::getline(ss, token, ' ')) {
        tokens.push_back(token);
    }
    dsm::LocalBufferKey lKey;
    dsm::RemoteBufferKey rKey;
    uint16_t size;
    switch (tokens.size()-1) {
        case 0:
            if (tokens[0] == "kill" || tokens[0] == "exit") {
                std::cout << "exiting" << std:: endl;
                isRunning = false;
                return;
            }
            break;
        case 1:
            lKey = dsm::Client::createLocalKey(tokens[1]);
            if (tokens[0] == "dcl") {
                if (client->disconnectFromLocalBuffer(lKey)) {
                    std::cout << "Disconnected from local buffer";
                } else {
                    std::cout << "Invalid operands";
                }
            } else if (tokens[0] == "checkl") {
                size = client->doesLocalExist(lKey);
                if (size) {
                    std::cout << "Local buffer exists with size " << size;
                } else {
                    std::cout << "Local buffer does not exist";
                }
            } else if (tokens[0] == "getl") {
                char data[MAX_BUFFER_SIZE];
                if (client->getLocalBufferContents(lKey, data)) {
                    std::cout << std::string(data, client->doesLocalExist(lKey));
                } else {
                    std::cout << "Invalid operands";
                }
            }
            break;
        case 2:
            lKey = dsm::Client::createLocalKey(tokens[1]);
            if (tokens[0] == "regl") {
                if (client->registerLocalBuffer(lKey, stoi(tokens[2]), false)) {
                    std::cout << "Registered new local buffer";
                } else {
                    std::cout << "Invalid operands";
                }
            } else if (tokens[0] == "reglo") {
                if (client->registerLocalBuffer(lKey, stoi(tokens[2]), true)) {
                    std::cout << "Registered new local only buffer";
                } else {
                    std::cout << "Invalid operands";
                }
            } else if (tokens[0] == "setl") {
                if (client->setLocalBufferContents(lKey, tokens[2].data())) {
                    std::cout << "Set local buffer contents";
                } else {
                    std::cout << "Invalid operands";
                }
            }
            break;
        case 3:
            rKey = dsm::Client::createRemoteKey(tokens[1], tokens[2], stoi(tokens[3]));
            if (tokens[0] == "regr") {
                if (client->registerRemoteBuffer(rKey)) {
                    std::cout << "Registered new remote buffer";
                } else {
                    std::cout << "Invalid operands";
                }
            } else if (tokens[0] == "dcr") {
                if (client->disconnectFromRemoteBuffer(rKey)) {
                    std::cout << "Disconnect from remote buffer";
                } else {
                    std::cout << "Invalid operands";
                }
            } else if (tokens[0] == "checkr") {
                size = client->doesRemoteExist(rKey);
                if (size) {
                    if (client->isRemoteActive(rKey)) {
                        std::cout << "Remote buffer is active and has size " << size;
                    } else {
                        std::cout << "Remote buffer is inactive and has size " << size;
                    }
                } else {
                    std::cout << "Remote buffer does not exist";
                }
            } else if (tokens[0] == "getr") {
                char data[MAX_BUFFER_SIZE];
                switch (client->getRemoteBufferContents(rKey, data)) {
                    case 0:
                        std::cout << "Active: " << std::string(data, client->doesRemoteExist(rKey));
                        break;
                    case 1:
                        std::cout << "Inactive: " << std::string(data, client->doesRemoteExist(rKey));
                        break;
                    default:
                        std::cout << "Invalid operands";
                        break;
                }
            }
            break;
        default:
            std::cout << "unknown";
            break;
    }
    std::cout << std::endl;
}

int main(int argc, char** argv) {
    try {
        int serverID;
        int clientID;
        po::options_description generalOptions("Options");
        generalOptions.add_options()
            ("help,h", "print help message")
            ("serverID,s", po::value<int>(&serverID), "the serverID of the server")
            ("clientID,c", po::value<int>(&clientID), "the clientID to connect with")
            ;
        po::positional_options_description positionalOptions;
        positionalOptions
            .add("serverID", 1)
            .add("clientID", 1)
            ;

        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).
                  options(generalOptions).
                  positional(positionalOptions).
                  run(), vm);
        po::notify(vm);

        if (vm.count("help") || !vm.count("serverID") || !vm.count("clientID")) {
            std::cout << "Usage: clientTester serverID clientID" << std::endl;
            std::cout << generalOptions;
            return 0;
        }

        if (serverID < 0 || serverID >= MAX_SERVERS) {
            std::cout << "Server ID must be between 0 and " << MAX_SERVERS-1 << std::endl;
            return 0;
        }

        if (clientID < 0 || clientID >= MAX_CLIENTS) {
            std::cout << "Client ID must be between 0 and " << MAX_CLIENTS-1 << std::endl;
            return 0;
        }

        std::signal(SIGINT, signalHandler);

        dsm::Client client(serverID, clientID);

        isRunning = true;
        while(isRunning.load()) {
            processInput(&client);
        }

        return 0;
    } catch (std::exception& e) {
        std::cout << "Error: ";
        std::cout << e.what() << std::endl;
        return 1;
    }

}
