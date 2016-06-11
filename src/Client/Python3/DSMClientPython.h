#ifndef DSMCLIENT_PYTHON_H
#define DSMCLIENT_PYTHON_H

#include <boost/python.hpp>

#include "../DSMClient.h"

using namespace boost::python;

BOOST_PYTHON_MODULE(pydsm) {
    class_<dsm::Client, boost::noncopyable>("Client", init<uint8_t, uint8_t, bool>())
        .def("registerLocalBuffer", &dsm::Client::registerLocalBuffer)
        .def("registerRemoteBuffer", &dsm::Client::registerRemoteBuffer)
        .def("disconnectFromLocalBuffer", &dsm::Client::disconnectFromLocalBuffer)
        .def("disconnectFromRemoteBuffer", &dsm::Client::disconnectFromRemoteBuffer)
        .def("doesLocalExist", &dsm::Client::doesLocalExist)
        .def("doesRemoteExist", &dsm::Client::doesRemoteExist)
        .def("getLocalBufferContents", &dsm::Client::PY_getLocalBufferContents)
        .def("setLocalBufferContents", &dsm::Client::PY_setLocalBufferContents)
        .def("getRemoteBufferContents", &dsm::Client::PY_getRemoteBufferContents)
        ;
}

std::string dsm::Client::PY_getLocalBufferContents(std::string name) {
    interprocess::sharable_lock<interprocess_sharable_mutex> mapLock(*_localBufferMapLock);
    auto iterator = _localBufferMap->find(name.c_str());
    if (iterator == _localBufferMap->end()) {
        return "";
    }
    interprocess::sharable_lock<interprocess_sharable_mutex> dataLock(*(std::get<2>(iterator->second).get()));
    void* ptr = _segment.get_address_from_handle(std::get<0>(iterator->second));
    uint16_t len = std::get<1>(iterator->second);
    return std::string((char*)ptr, len);
}

bool dsm::Client::PY_setLocalBufferContents(std::string name, std::string data) {
    interprocess::sharable_lock<interprocess_sharable_mutex> mapLock(*_localBufferMapLock);
    auto iterator = _localBufferMap->find(name.c_str());
    if (iterator == _localBufferMap->end()) {
        return false;
    }
    interprocess::scoped_lock<interprocess_sharable_mutex> dataLock(*(std::get<2>(iterator->second).get()));
    void* ptr = _segment.get_address_from_handle(std::get<0>(iterator->second));
    uint16_t len = std::get<1>(iterator->second);
    memcpy(ptr, data.data(), len);
    return true;
}

std::string dsm::Client::PY_getRemoteBufferContents(std::string name, std::string ipaddr, uint8_t serverID) {
    interprocess::sharable_lock<interprocess_sharable_mutex> mapLock(*_remoteBufferMapLock);
    RemoteBufferKey key(name.c_str(), ip::udp::endpoint(ip::address::from_string(ipaddr), RECEIVER_BASE_PORT+serverID));
    auto iterator = _remoteBufferMap->find(key);
    if (iterator == _remoteBufferMap->end()) {
        return "";
    }
    interprocess::sharable_lock<interprocess_sharable_mutex> dataLock(*(std::get<2>(iterator->second).get()));
    void* ptr = _segment.get_address_from_handle(std::get<0>(iterator->second));
    uint16_t len = std::get<1>(iterator->second);
    return std::string((char*)ptr, len);
}

#endif //DSMCLIENT_PYTHON_H
