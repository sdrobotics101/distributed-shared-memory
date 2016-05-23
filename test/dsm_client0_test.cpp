#include <iostream>
#include "../src/Client/DSMClient.h"

int main() {
    std::cout << "Starting" << std::endl;
    dsm::Client _client("server0", 0);
    _client.registerRemoteBuffer("remote0", "127.0.0.1", 1);
    std::cout << "Done" << std::endl;
}
