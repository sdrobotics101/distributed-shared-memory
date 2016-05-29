#include <iostream>
#include <csignal>

#include "../src/Server/DSMServer.h"

dsm::Server* _server;

void signalHandler(int signum) {
    _server->stop();
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "USAGE: ./Server ServerID";
    }
    _server = new dsm::Server(std::stoi(argv[1]));
    std::signal(SIGINT, signalHandler);
    _server->start();
    delete _server;
}
