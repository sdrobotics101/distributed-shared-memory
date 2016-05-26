#include <iostream>
#include <thread>
#include "../src/Client/DSMClient.h"

int main() {
    dsm::Client _client("server0", 0);
    _client.registerRemoteBuffer("remote0", "127.0.0.1", 1);
    char data[5] = "";
    data[4] = '\0';
    /* std::string word; */
    /* word.resize(4); */
    /* word = ""; */
    while (strcmp(data, "kill") != 0) {
        _client.getRemoteBufferContents("remote0", "127.0.0.1", data);
        std::cout << "WORD: " << data << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}
