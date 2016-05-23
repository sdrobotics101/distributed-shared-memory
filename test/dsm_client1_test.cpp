#include <iostream>
#include "../src/Client/DSMClient.h"

int main() {
    std::cout << "Starting" << std::endl;
    dsm::Client _client("server1", 0);
    _client.registerLocalBuffer("remote0", 17);
    _client.registerLocalBuffer("remote1", 20);
    std::string input;
    std::getline(std::cin, input);
    _client.setLocalBufferContents("remote0", input.data());
    std::cout << "Done" << std::endl;
}
