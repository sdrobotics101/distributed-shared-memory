#include <iostream>
#include "../include/DSMServer.h"

int main(int argc, char** argv) {
    std::cout << "Starting" << std::endl;
    std::string name;
    if (argc > 1) {
        name = argv[1];
    } else {
        name = "server";
    }
    DSMServer _server(name);
    _server.start();
    std::cout << "Done" << std::endl;
}
