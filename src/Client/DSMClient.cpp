#include "DSMClient.h"

#ifdef BUILD_PYTHON_MODULE
#include "DSMClientPython.h"
#endif

dsm::Client::Client(uint8_t serverID, uint8_t clientID, bool reset) : Base("server"+std::to_string((serverID < 0 || serverID > 15) ? 0 : serverID)),
                                                                      _clientID(clientID) {
    _clientID &= 0x0F;    //only use the lower 4 bits
    if (reset) {
        _message.reset();
        _message.header = _clientID;
        _message.header |= (DISCONNECT_CLIENT << 4);
        _messageQueue.send(&_message, MESSAGE_SIZE, 0);
    }
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
    _message.header |= (FETCH_REMOTE << 4);
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

bool dsm::Client::disconnectFromRemoteBuffer(std::string name, std::string ipaddr, uint8_t portOffset) {
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
    _message.header |= (DISCONNECT_REMOTE << 4);
    _message.header |= (portOffset << 8);
    std::strcpy(_message.name, name.c_str());

    _messageQueue.send(&_message, MESSAGE_SIZE, 0);
    return true;
}

bool dsm::Client::doesLocalExist(std::string name) {
    interprocess::sharable_lock<interprocess_sharable_mutex> mapLock(*_localBufferMapLock);
    return (bool)_localBufferMap->count(name);
}

bool dsm::Client::doesRemoteExist(std::string name, std::string ipaddr, uint8_t portOffset) {
    interprocess::sharable_lock<interprocess_sharable_mutex> mapLock(*_remoteBufferMapLock);
    RemoteBufferKey key(name, ip::udp::endpoint(ip::address::from_string(ipaddr), REQUEST_BASE_PORT+portOffset));
    return (bool)_remoteBufferMap->count(key);
}

bool dsm::Client::getLocalBufferContents(std::string name, void* data) {
    interprocess::sharable_lock<interprocess_sharable_mutex> mapLock(*_localBufferMapLock);
    auto iterator = _localBufferMap->find(name);
    if (iterator == _localBufferMap->end()) {
        return false;
    }
    interprocess::sharable_lock<interprocess_sharable_mutex> dataLock(*(std::get<2>(iterator->second).get()));
    void* ptr = _segment.get_address_from_handle(std::get<0>(iterator->second));
    uint16_t len = std::get<1>(iterator->second);
    memcpy(data, ptr, len);
    return true;
}

std::string dsm::Client::getLocalBufferContents(std::string name) {
    interprocess::sharable_lock<interprocess_sharable_mutex> mapLock(*_localBufferMapLock);
    auto iterator = _localBufferMap->find(name);
    if (iterator == _localBufferMap->end()) {
        return "";
    }
    interprocess::sharable_lock<interprocess_sharable_mutex> dataLock(*(std::get<2>(iterator->second).get()));
    void* ptr = _segment.get_address_from_handle(std::get<0>(iterator->second));
    uint16_t len = std::get<1>(iterator->second);
    return std::string((char*)ptr, len);
}

bool dsm::Client::setLocalBufferContents(std::string name, const void* data) {
    interprocess::sharable_lock<interprocess_sharable_mutex> mapLock(*_localBufferMapLock);
    auto iterator = _localBufferMap->find(name);
    if (iterator == _localBufferMap->end()) {
        return false;
    }
    interprocess::scoped_lock<interprocess_sharable_mutex> dataLock(*(std::get<2>(iterator->second).get()));
    void* ptr = _segment.get_address_from_handle(std::get<0>(iterator->second));
    uint16_t len = std::get<1>(iterator->second);
    memcpy(ptr, data, len);
    return true;
}

bool dsm::Client::setLocalBufferContents(std::string name, std::string data) {
    interprocess::sharable_lock<interprocess_sharable_mutex> mapLock(*_localBufferMapLock);
    auto iterator = _localBufferMap->find(name);
    if (iterator == _localBufferMap->end()) {
        return false;
    }
    interprocess::scoped_lock<interprocess_sharable_mutex> dataLock(*(std::get<2>(iterator->second).get()));
    void* ptr = _segment.get_address_from_handle(std::get<0>(iterator->second));
    uint16_t len = std::get<1>(iterator->second);
    memcpy(ptr, data.data(), len);
    return true;
}

bool dsm::Client::getRemoteBufferContents(std::string name, std::string ipaddr, uint8_t portOffset, void* data) {
    interprocess::sharable_lock<interprocess_sharable_mutex> mapLock(*_remoteBufferMapLock);
    RemoteBufferKey key(name, ip::udp::endpoint(ip::address::from_string(ipaddr), REQUEST_BASE_PORT+portOffset));
    auto iterator = _remoteBufferMap->find(key);
    if (iterator == _remoteBufferMap->end()) {
        return false;
    }
    interprocess::sharable_lock<interprocess_sharable_mutex> dataLock(*(std::get<2>(iterator->second).get()));
    void* ptr = _segment.get_address_from_handle(std::get<0>(iterator->second));
    uint16_t len = std::get<1>(iterator->second);
    memcpy(data, ptr, len);
    return true;
}

std::string dsm::Client::getRemoteBufferContents(std::string name, std::string ipaddr, uint8_t portOffset) {
    interprocess::sharable_lock<interprocess_sharable_mutex> mapLock(*_remoteBufferMapLock);
    RemoteBufferKey key(name, ip::udp::endpoint(ip::address::from_string(ipaddr), REQUEST_BASE_PORT+portOffset));
    auto iterator = _remoteBufferMap->find(key);
    if (iterator == _remoteBufferMap->end()) {
        return "";
    }
    interprocess::sharable_lock<interprocess_sharable_mutex> dataLock(*(std::get<2>(iterator->second).get()));
    void* ptr = _segment.get_address_from_handle(std::get<0>(iterator->second));
    uint16_t len = std::get<1>(iterator->second);
    return std::string((char*)ptr, len);
}
