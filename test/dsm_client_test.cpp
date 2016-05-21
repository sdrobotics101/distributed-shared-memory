#include <iostream>
#include "../src/Client/DSMClient.h"

int main(int argc, char** argv) {
    std::cout << "Starting" << std::endl;
    std::string name;
    if (argc > 1) {
        name = argv[1];
    } else {
        name = "server";
    }
    dsm::Client _client(name, 0);
    _client.registerRemoteBuffer("remote0", "127.0.0.1", 1);

    _client.registerLocalBuffer("end", 30);
    std::cout << "Done" << std::endl;
}
