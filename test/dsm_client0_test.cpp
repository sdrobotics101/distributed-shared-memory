#include <iostream>
#include "../src/Client/DSMClient.h"

int main() {
    dsm::Client _client("server0", 0);
    _client.registerRemoteBuffer("remote0", "127.0.0.1", 1);
    _client.registerRemoteBuffer("remote1", "127.0.0.1", 1);
}
