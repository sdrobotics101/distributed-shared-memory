#include <iostream>
#include <csignal>

#include "../src/Server/DSMServer.h"

dsm::Server _server("server1", 1, "239.255.0.2", 40000);

void signalHandler(int signum) {
    std::cout << "got signal: " << signum << std::endl;
    _server.stop();
}

int main() {
    std::cout << "Starting" << std::endl;
    std::signal(SIGINT, signalHandler);
    _server.start();
    std::cout << "Done" << std::endl;
}
