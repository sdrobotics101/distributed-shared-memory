#include "DSMClient.h"

#ifdef BUILD_PYTHON_MODULE
#include "Python3/DSMClientPython.h"
#endif

dsm::Client::Client(uint8_t serverID, uint8_t clientID, bool reset) : Base("server"+std::to_string(serverID)),
                                                                      _clientID(clientID) {
    _message.clientID = _clientID;
    if (reset) {
        _message.options = CONNECT_CLIENT;
    } else {
        _message.options = CONNECT_CLIENT_NORESET;
    }
    _messageQueue.send(&_message, QUEUE_MESSAGE_SIZE, 0);
}

dsm::Client::~Client() {
    _message.options = DISCONNECT_CLIENT;
    _messageQueue.send(&_message, QUEUE_MESSAGE_SIZE, 0);
}

LocalBufferKey dsm::Client::createLocalKey(std::string name) {
    return LocalBufferKey(name.c_str());
}

RemoteBufferKey dsm::Client::createRemoteKey(std::string name, std::string ipaddr, uint8_t serverID) {
    return RemoteBufferKey(name.c_str(), ip::udp::endpoint(ip::address::from_string(ipaddr), RECEIVER_BASE_PORT+serverID));
}

bool dsm::Client::registerLocalBuffer(LocalBufferKey key, uint16_t length, bool localOnly) {
    if (length < 1 || length > MAX_BUFFER_SIZE) {
        return false;
    }
    if (localOnly) {
        _message.options = CREATE_LOCALONLY;
    } else {
        _message.options = CREATE_LOCAL;
    }
    std::strcpy(_message.name, key.name);
    _message.footer.size = length;

    _messageQueue.send(&_message, QUEUE_MESSAGE_SIZE, 0);
    return true;
}

bool dsm::Client::registerRemoteBuffer(RemoteBufferKey key) {
    _message.footer.ipaddr = key.endpoint.address().to_v4().to_ulong();
    _message.options = FETCH_REMOTE;
    _message.serverID = key.endpoint.port()-RECEIVER_BASE_PORT;
    std::strcpy(_message.name, key.name);

    _messageQueue.send(&_message, QUEUE_MESSAGE_SIZE, 0);
    return true;
}

bool dsm::Client::disconnectFromLocalBuffer(LocalBufferKey key) {
    _message.options = DISCONNECT_LOCAL;
    std::strcpy(_message.name, key.name);

    _messageQueue.send(&_message, QUEUE_MESSAGE_SIZE, 0);
    return true;
}

bool dsm::Client::disconnectFromRemoteBuffer(RemoteBufferKey key) {
    _message.footer.ipaddr = key.endpoint.address().to_v4().to_ulong();
    _message.options = DISCONNECT_REMOTE;
    _message.serverID = key.endpoint.port()-RECEIVER_BASE_PORT;
    std::strcpy(_message.name, key.name);

    _messageQueue.send(&_message, QUEUE_MESSAGE_SIZE, 0);
    return true;
}

uint16_t dsm::Client::doesLocalExist(LocalBufferKey key) {
    interprocess::sharable_lock<interprocess_sharable_mutex> mapLock(*_localBufferMapLock);
    auto iterator = _localBufferMap->find(key);
    if (iterator == _localBufferMap->end()) {
        return 0;
    }
    return std::get<1>(iterator->second);
}

uint16_t dsm::Client::doesRemoteExist(RemoteBufferKey key) {
    interprocess::sharable_lock<interprocess_sharable_mutex> mapLock(*_remoteBufferMapLock);
    auto iterator = _remoteBufferMap->find(key);
    if (iterator == _remoteBufferMap->end()) {
        return 0;
    }
    return std::get<1>(iterator->second);
}

bool dsm::Client::isRemoteActive(RemoteBufferKey key) {
    interprocess::sharable_lock<interprocess_sharable_mutex> mapLock(*_remoteBufferMapLock);
    auto iterator = _remoteBufferMap->find(key);
    if (iterator == _remoteBufferMap->end()) {
        return false;
    }
    return std::get<3>(iterator->second);
}

bool dsm::Client::getLocalBufferContents(LocalBufferKey key, void* data) {
    interprocess::sharable_lock<interprocess_sharable_mutex> mapLock(*_localBufferMapLock);
    auto iterator = _localBufferMap->find(key);
    if (iterator == _localBufferMap->end()) {
        return false;
    }
    interprocess::sharable_lock<interprocess_sharable_mutex> dataLock(*(std::get<2>(iterator->second).get()));
    void* ptr = _segment.get_address_from_handle(std::get<0>(iterator->second));
    uint16_t len = std::get<1>(iterator->second);
    memcpy(data, ptr, len);
    return true;
}

bool dsm::Client::setLocalBufferContents(LocalBufferKey key, const void* data) {
    interprocess::sharable_lock<interprocess_sharable_mutex> mapLock(*_localBufferMapLock);
    auto iterator = _localBufferMap->find(key);
    if (iterator == _localBufferMap->end()) {
        return false;
    }
    interprocess::scoped_lock<interprocess_sharable_mutex> dataLock(*(std::get<2>(iterator->second).get()));
    void* ptr = _segment.get_address_from_handle(std::get<0>(iterator->second));
    uint16_t len = std::get<1>(iterator->second);
    memcpy(ptr, data, len);
    return true;
}

bool dsm::Client::getRemoteBufferContents(RemoteBufferKey key, void* data) {
    interprocess::sharable_lock<interprocess_sharable_mutex> mapLock(*_remoteBufferMapLock);
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
