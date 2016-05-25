#include <iostream>
#include <csignal>

#include "../src/Server/DSMServer.h"

dsm::Server _server("server1", 1, "239.255.0.2", 40000);

void signalHandler(int signum) {
    _server.stop();
}

int main() {
    std::signal(SIGINT, signalHandler);
    _server.start();
}
