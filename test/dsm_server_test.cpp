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
    DSMServer _server(name, 8888);
    std::cout << "Done" << std::endl;
}
