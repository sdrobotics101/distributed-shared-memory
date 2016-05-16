#include <iostream>
#include "../include/DSMClient.h"

int main(int argc, char** argv) {
    std::cout << "Starting" << std::endl;
    std::string name;
    if (argc > 1) {
        name = argv[1];
    } else {
        name = "server";
    }
    DSMClient _client(name);
    std::cout << "Done" << std::endl;
}
