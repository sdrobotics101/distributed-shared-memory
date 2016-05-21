#include "DSMClient.h"

dsm::Client::Client(std::string name, uint8_t clientID) : Base(name),
                                                          _clientID(clientID) {
    _clientID &= 0b00001111;    //only use the lower 4 bits
}

dsm::Client::~Client() {}

bool dsm::Client::registerLocalBuffer(std::string name, uint16_t length) {
    if (name.length() > 26) {
        return false;
    }
    _message.reset();
    _message.header = _clientID;
    //message type is 0000
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

    _message.header = _clientID;
    _message.header |= (CREATE_REMOTE << 4);
    std::strcpy(_message.name, name.c_str());

    _messageQueue.send(&_message, MESSAGE_SIZE, 0);
    return true;
}


bool dsm::Client::disconnectFromBuffer(std::string name) {
    if (name.length() > 26) {
        return false;
    }

    _message.reset();
    _message.header = _clientID;
    _message.header |= (DISCONNECT_LOCAL << 4);
    std::strcpy(_message.name, name.c_str());

    _messageQueue.send(&_message, MESSAGE_SIZE, 0);
    return true;
}
