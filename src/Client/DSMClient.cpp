#include "DSMClient.h"

dsm::Client::Client(std::string name) : Base(name) {}

dsm::Client::~Client() {}

bool dsm::Client::registerLocalBuffer(std::string name, uint16_t length) {
    if (name.length() > 26) {
        return false;
    }
    _message.reset();
    _message.header = 0;
    std::strcpy(_message.name, name.c_str());
    _message.footer.size = length;

    _messageQueue.send(&_message, MESSAGE_SIZE, 0);
    return true;
}

bool dsm::Client::registerRemoteBuffer(std::string name, std::string ipaddr) {
    if (name.length() > 26) {
        return false;
    }
    _message.reset();
    if (inet_aton(ipaddr.c_str(), &_message.footer.ipaddr) == 0) {
        return false;
    }

    _message.header = 1;
    std::strcpy(_message.name, name.c_str());

    _messageQueue.send(&_message, MESSAGE_SIZE, 0);
    return true;
}
