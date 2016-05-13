#include <iostream>
#include "../include/DSMClient.h"

int main() {
    volatile DSMClient _client("serv");
    std::cout << "created" << std::endl;
}
