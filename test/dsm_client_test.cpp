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
    dsm::Client _client(name, 1);
    dsm::Client _client2(name, 2);
    _client.registerLocalBuffer("name0", 18);
    _client.registerLocalBuffer("name0", 17);
    _client.registerLocalBuffer("name1", 20);
    _client.registerLocalBuffer("name1", 22);
    _client.registerRemoteBuffer("remote0", "192.168.1.1");
    _client.registerRemoteBuffer("remote1", "hello");

    _client2.registerLocalBuffer("name1", 20);
    _client.disconnectFromBuffer("name1");
    _client2.disconnectFromBuffer("name1");




    _client.registerLocalBuffer("end", 30);
    std::cout << "Done" << std::endl;
}
