#include <iostream>
#include <thread>
#include "../src/Client/DSMClient.h"

int main() {
    dsm::Client _client("server1", 0);
    _client.registerLocalBuffer("remote0", 3);
    _client.registerLocalBuffer("remote1", 5);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    _client.setLocalBufferContents("remote0", "zzz");
    _client.setLocalBufferContents("remote1", "hello");
}
