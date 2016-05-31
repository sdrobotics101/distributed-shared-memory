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
    interprocess::sharable_lock<interprocess_upgradable_mutex> mapLock(*_localBufferMapLock);
    try {
        _localBufferMap->at(name);
        return true;
    } catch (std::exception const& e) {
        return false;
    }
}

bool dsm::Client::doesRemoteExist(std::string name, std::string ipaddr, uint8_t portOffset) {
    interprocess::sharable_lock<interprocess_upgradable_mutex> mapLock(*_remoteBufferMapLock);
    try {
        RemoteBufferKey key(name, ip::udp::endpoint(ip::address::from_string(ipaddr), REQUEST_BASE_PORT+portOffset));
        RemoteBuffer buf = _remoteBufferMap->at(key);
        return true;
    } catch (std::exception const& e) {
        return false;
    }
}

bool dsm::Client::getLocalBufferContents(std::string name, void* data) {
    interprocess::sharable_lock<interprocess_upgradable_mutex> mapLock(*_localBufferMapLock);
    try {
        LocalBuffer buf = _localBufferMap->at(name);
        interprocess::sharable_lock<interprocess_upgradable_mutex> dataLock(*(std::get<2>(buf).get()));
        void* ptr = _segment.get_address_from_handle(std::get<0>(buf));
        uint16_t len = std::get<1>(buf);
        memcpy(data, ptr, len);
        return true;
    } catch (std::exception const& e) {
        return false;
    }
}

bool dsm::Client::setLocalBufferContents(std::string name, const void* data) {
    interprocess::sharable_lock<interprocess_upgradable_mutex> mapLock(*_localBufferMapLock);
    try {
        LocalBuffer buf = _localBufferMap->at(name);
        interprocess::scoped_lock<interprocess_upgradable_mutex> dataLock(*(std::get<2>(buf).get()));
        void* ptr = _segment.get_address_from_handle(std::get<0>(buf));
        uint16_t len = std::get<1>(buf);
        memcpy(ptr, data, len);
        return true;
    } catch (std::exception const& e) {
        return false;
    }
}

bool dsm::Client::getRemoteBufferContents(std::string name, std::string ipaddr, uint8_t portOffset, void* data) {
    interprocess::sharable_lock<interprocess_upgradable_mutex> mapLock(*_remoteBufferMapLock);
    try {
        RemoteBufferKey key(name, ip::udp::endpoint(ip::address::from_string(ipaddr), REQUEST_BASE_PORT+portOffset));
        RemoteBuffer buf = _remoteBufferMap->at(key);
        interprocess::sharable_lock<interprocess_upgradable_mutex> dataLock(*(std::get<2>(buf).get()));
        void* ptr = _segment.get_address_from_handle(std::get<0>(buf));
        uint16_t len = std::get<1>(buf);
        memcpy(data, ptr, len);
        return true;
    } catch (std::exception const& e) {
        return false;
    }
}

#ifdef BUILD_PYTHON_MODULE
#include "DSMClientPython.h"
#endif
