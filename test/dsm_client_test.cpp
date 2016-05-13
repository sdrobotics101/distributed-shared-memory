#include <iostream>
#include "../include/Client.h"

int main() {
    volatile DSMClient _client("serv");
    std::cout << "created" << std::endl;
}
