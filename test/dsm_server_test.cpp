#include <iostream>
#include <csignal>

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/scoped_ptr.hpp>

#include "../src/Server/DSMServer.h"

boost::scoped_ptr<dsm::Server> _server;

void signalHandler(int signum) {
    _server->stop();
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "USAGE: ./Server ServerID";
    }
    _server.reset(new dsm::Server(std::stoi(argv[1])));
    std::signal(SIGINT, signalHandler);
    _server->start();
}
