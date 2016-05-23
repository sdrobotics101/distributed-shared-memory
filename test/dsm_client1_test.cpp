#include <iostream>
#include "../src/Client/DSMClient.h"

int main() {
    std::cout << "Starting" << std::endl;
    dsm::Client _client("server1", 0);
    _client.registerLocalBuffer("remote0", 17);
    std::cout << "Done" << std::endl;
}
