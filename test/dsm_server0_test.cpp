#include <iostream>
#include <csignal>

#include "../src/Server/DSMServer.h"

dsm::Server _server("server0", 0, "239.255.0.1", 30000);

void signalHandler(int signum) {
    _server.stop();
}

int main() {
    std::signal(SIGINT, signalHandler);
    _server.start();
}
