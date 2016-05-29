#include "DSMClient.h"

dsm::Client::Client(uint8_t serverID, uint8_t clientID) : Base("server"+std::to_string(serverID)),
                                                          _clientID(clientID) {
    _clientID &= 0x0F;    //only use the lower 4 bits
}

dsm::Client::~Client() {
    _message.reset();
    _message.header = _clientID;
    _message.header |= (DISCONNECT_CLIENT << 4);
    _messageQueue.send(&_message, MESSAGE_SIZE, 0);
}

bool dsm::Client::registerLocalBuffer(std::string name, uint16_t length, bool localOnly) {
    if (length < 1 || length > MAX_BUFFER_SIZE) {
        return false;
    }
    if (name.length() > 26) {
        return false;
    }
    _message.reset();
    _message.header = _clientID;
    if (localOnly) {
        _message.header |= (CREATE_LOCALONLY << 4);
    }
    //message type is 0000 otherwise
    std::strcpy(_message.name, name.c_str());
    _message.footer.size = length;

    _messageQueue.send(&_message, MESSAGE_SIZE, 0);
    return true;
}

bool dsm::Client::registerRemoteBuffer(std::string name, std::string ipaddr, uint8_t portOffset) {
    if (portOffset < 0 || portOffset > 15) {
        return false;
    }
    if (name.length() > 26) {
        return false;
    }
    _message.reset();
    if (inet_aton(ipaddr.c_str(), &_message.footer.ipaddr) == 0) {
        return false;
    }

    _message.header = _clientID;
    _message.header |= (CREATE_REMOTE << 4);
    _message.header |= (portOffset << 8);

    std::strcpy(_message.name, name.c_str());

    _messageQueue.send(&_message, MESSAGE_SIZE, 0);
    return true;
}


bool dsm::Client::disconnectFromLocalBuffer(std::string name) {
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

bool dsm::Client::disconnectFromRemoteBuffer(std::string name, std::string ipaddr) {
    if (name.length() > 26) {
        return false;
    }

    _message.reset();
    if (inet_aton(ipaddr.c_str(), &_message.footer.ipaddr) == 0) {
        return false;
    }

    _message.header = _clientID;
    _message.header |= (DISCONNECT_REMOTE << 4);
    std::strcpy(_message.name, name.c_str());

    _messageQueue.send(&_message, MESSAGE_SIZE, 0);
    return true;
}

bool dsm::Client::getRemoteBufferContents(std::string name, std::string ipaddr, void* data) {
    sharable_lock<interprocess_upgradable_mutex> mapLock(*_remoteBufferMapLock);
    try {
        Buffer buf = _remoteBufferMap->at(ipaddr+name);
        sharable_lock<interprocess_upgradable_mutex> dataLock(*(std::get<2>(buf).get()));
        void* ptr = _segment.get_address_from_handle(std::get<0>(buf));
        uint16_t len = std::get<1>(buf);
        memcpy(data, ptr, len);
        return true;
    } catch (boost::exception const& e) {
        return false;
    }
}

bool dsm::Client::getLocalBufferContents(std::string name, void* data) {
    sharable_lock<interprocess_upgradable_mutex> mapLock(*_localBufferMapLock);
    try {
        Buffer buf = _localBufferMap->at(name);
        sharable_lock<interprocess_upgradable_mutex> dataLock(*(std::get<2>(buf).get()));
        void* ptr = _segment.get_address_from_handle(std::get<0>(buf));
        uint16_t len = std::get<1>(buf);
        memcpy(data, ptr, len);
        return true;
    } catch (boost::exception const& e) {
        return false;
    }
}

bool dsm::Client::setLocalBufferContents(std::string name, const void* data) {
    sharable_lock<interprocess_upgradable_mutex> mapLock(*_localBufferMapLock);
    try {
        Buffer buf = _localBufferMap->at(name);
        scoped_lock<interprocess_upgradable_mutex> dataLock(*(std::get<2>(buf).get()));
        void* ptr = _segment.get_address_from_handle(std::get<0>(buf));
        uint16_t len = std::get<1>(buf);
        memcpy(ptr, data, len);
        return true;
    } catch (boost::exception const& e) {
        return false;
    }
}
