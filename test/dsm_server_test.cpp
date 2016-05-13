#include <iostream>
#include "../include/Server.h"

int main() {
    DSMServer _server("serv");
    std::cout << "created" << std::endl;
    _server.start();
}
