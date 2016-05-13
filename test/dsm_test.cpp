#include <iostream>
#include <chrono>
#include <thread>
#include "../include/Client.h"
#include "../include/Server.h"

int main() {
    DSMServer _server("serv");
    std::cout << "created" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    {
        DSMClient _client("serv");
        std::cout << "created" << std::endl;
    }
    std::cout << "dumping"<< std::endl;
    _server.dump();
}
