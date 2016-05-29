#include <iostream>
#include <csignal>

#include "../src/Server/DSMServer.h"

dsm::Server _server(1);

void signalHandler(int signum) {
    _server.stop();
}

int main() {
    std::signal(SIGINT, signalHandler);
    _server.start();
}
