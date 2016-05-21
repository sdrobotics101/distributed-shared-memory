#include <iostream>
#include "../src/Server/DSMServer.h"

int main(int argc, char** argv) {
    std::cout << "Starting" << std::endl;
    std::string name;
    if (argc > 1) {
        name = argv[1];
    } else {
        name = "server";
    }
    dsm::Server _server(name, 0);
    dsm::Server _server2("server2", 1);
    _server2.start();
    _server.start();
    std::cout << "Done" << std::endl;
}
