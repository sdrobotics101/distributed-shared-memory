#ifndef DSMCLIENT_PYTHON_H
#define DSMCLIENT_PYTHON_H

#include <boost/python.hpp>

#include "../DSMClient.h"

namespace python = boost::python;

BOOST_PYTHON_MODULE(pydsm) {
    python::class_<dsm::Client, boost::noncopyable>("Client", python::init<uint8_t, uint8_t, bool>())
        .def("registerLocalBuffer", &dsm::Client::PY_registerLocalBuffer)
        .def("registerRemoteBuffer", &dsm::Client::PY_registerRemoteBuffer)
        .def("disconnectFromLocalBuffer", &dsm::Client::PY_disconnectFromLocalBuffer)
        .def("disconnectFromRemoteBuffer", &dsm::Client::PY_disconnectFromRemoteBuffer)
        .def("doesLocalExist", &dsm::Client::PY_doesLocalExist)
        .def("doesRemoteExist", &dsm::Client::PY_doesRemoteExist)
        .def("isRemoteActive", &dsm::Client::PY_isRemoteActive)
        .def("getLocalBufferContents", &dsm::Client::PY_getLocalBufferContents)
        .def("setLocalBufferContents", &dsm::Client::PY_setLocalBufferContents)
        .def("getRemoteBufferContents", &dsm::Client::PY_getRemoteBufferContents)
        ;
}

bool dsm::Client::PY_registerLocalBuffer(std::string name, uint16_t length, bool localOnly) {
    return registerLocalBuffer(createLocalKey(name), length, localOnly);
}

bool dsm::Client::PY_registerRemoteBuffer(std::string name, std::string ipaddr, uint8_t serverID) {
    return registerRemoteBuffer(createRemoteKey(name, ipaddr, serverID));
}

bool dsm::Client::PY_disconnectFromLocalBuffer(std::string name) {
    return disconnectFromLocalBuffer(createLocalKey(name));
}

bool dsm::Client::PY_disconnectFromRemoteBuffer(std::string name, std::string ipaddr, uint8_t serverID) {
    return disconnectFromRemoteBuffer(createRemoteKey(name, ipaddr, serverID));
}

uint16_t dsm::Client::PY_doesLocalExist(std::string name) {
    return doesLocalExist(createLocalKey(name));
}

uint16_t dsm::Client::PY_doesRemoteExist(std::string name, std::string ipaddr, uint8_t serverID) {
    return doesRemoteExist(createRemoteKey(name, ipaddr, serverID));
}

bool dsm::Client::PY_isRemoteActive(std::string name, std::string ipaddr, uint8_t serverID) {
    return isRemoteActive(createRemoteKey(name, ipaddr, serverID));
}

python::list dsm::Client::PY_getLocalBufferContents(std::string name) {
    interprocess::sharable_lock<interprocess_sharable_mutex> mapLock(*_localBufferMapLock);
    auto iterator = _localBufferMap->find(createLocalKey(name));
    python::list buf;
    if (iterator == _localBufferMap->end()) {
        return buf;
    }
    interprocess::sharable_lock<interprocess_sharable_mutex> dataLock(*(std::get<2>(iterator->second).get()));
    void* ptr = _segment.get_address_from_handle(std::get<0>(iterator->second));
    uint16_t len = std::get<1>(iterator->second);
    for (int i = 0; i < len; i++) {
        buf.append<uint8_t>(*(((uint8_t*)ptr)+i));
    }
    return buf;
}

bool dsm::Client::PY_setLocalBufferContents(std::string name, std::string data) {
    interprocess::sharable_lock<interprocess_sharable_mutex> mapLock(*_localBufferMapLock);
    auto iterator = _localBufferMap->find(createLocalKey(name));
    if (iterator == _localBufferMap->end()) {
        return false;
    }
    interprocess::scoped_lock<interprocess_sharable_mutex> dataLock(*(std::get<2>(iterator->second).get()));
    void* ptr = _segment.get_address_from_handle(std::get<0>(iterator->second));
    uint16_t len = std::get<1>(iterator->second);
    memcpy(ptr, data.data(), len);
    return true;
}

python::tuple dsm::Client::PY_getRemoteBufferContents(std::string name, std::string ipaddr, uint8_t serverID) {
    interprocess::sharable_lock<interprocess_sharable_mutex> mapLock(*_remoteBufferMapLock);
    auto iterator = _remoteBufferMap->find(createRemoteKey(name, ipaddr, serverID));
    python::list buf;
    if (iterator == _remoteBufferMap->end()) {
        return python::make_tuple(buf, false);
    }
    interprocess::sharable_lock<interprocess_sharable_mutex> dataLock(*(std::get<2>(iterator->second).get()));
    void* ptr = _segment.get_address_from_handle(std::get<0>(iterator->second));
    uint16_t len = std::get<1>(iterator->second);
    for (int i = 0; i < len; i++) {
        buf.append<uint8_t>(*(((uint8_t*)ptr)+i));
    }
    return python::make_tuple(buf, std::get<3>(iterator->second));
}

#endif //DSMCLIENT_PYTHON_H
