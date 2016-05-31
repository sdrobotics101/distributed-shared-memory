#ifndef DSMCLIENT_PYTHON_H
#define DSMCLIENT_PYTHON_H

#include <boost/python.hpp>

#include "DSMClient.h"

using namespace boost::python;

BOOST_PYTHON_MODULE(dsmclient) {
    class_<RemoteBufferKey>("RemoteBufferKey", init<std::string, ip::udp::endpoint>())
        ;
    class_<dsm::Client, bases<dsm::Base>, boost::noncopyable>("DSMClient", init<uint8_t, uint8_t>())
        .def("registerLocalBuffer", &dsm::Client::registerLocalBuffer)
        .def("registerRemoteBuffer", &dsm::Client::registerRemoteBuffer)
        .def("disconnectFromLocalBuffer", &dsm::Client::disconnectFromLocalBuffer)
        .def("disconnectFromRemoteBuffer", &dsm::Client::disconnectFromRemoteBuffer)
        .def("doesLocalExist", &dsm::Client::doesLocalExist)
        .def("doesRemoteExist", &dsm::Client::doesRemoteExist)
        .def("getLocalBufferContents", &dsm::Client::getLocalBufferContents)
        .def("setLocalBufferContents", &dsm::Client::setLocalBufferContents)
        .def("getRemoteBufferContents", &dsm::Client::getRemoteBufferContents)
        ;
}

#endif //DSMCLIENT_PYTHON_H
