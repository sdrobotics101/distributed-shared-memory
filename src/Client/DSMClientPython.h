#ifndef DSMCLIENT_PYTHON_H
#define DSMCLIENT_PYTHON_H

#include <boost/python.hpp>

#include "DSMClient.h"

using namespace boost::python;

std::string (dsm::Client::*getLocalBuffer)(std::string) = &dsm::Client::getLocalBufferContents;
bool (dsm::Client::*setLocalBuffer)(std::string, std::string) = &dsm::Client::setLocalBufferContents;
std::string (dsm::Client::*getRemoteBuffer)(std::string, std::string, uint8_t) = &dsm::Client::getRemoteBufferContents;

BOOST_PYTHON_MODULE(pydsm) {
    class_<dsm::Client, boost::noncopyable>("Client", init<uint8_t, uint8_t>())
        .def("registerLocalBuffer", &dsm::Client::registerLocalBuffer)
        .def("registerRemoteBuffer", &dsm::Client::registerRemoteBuffer)
        .def("disconnectFromLocalBuffer", &dsm::Client::disconnectFromLocalBuffer)
        .def("disconnectFromRemoteBuffer", &dsm::Client::disconnectFromRemoteBuffer)
        .def("doesLocalExist", &dsm::Client::doesLocalExist)
        .def("doesRemoteExist", &dsm::Client::doesRemoteExist)
        .def("getLocalBufferContents", getLocalBuffer)
        .def("setLocalBufferContents", setLocalBuffer)
        .def("getRemoteBufferContents", getRemoteBuffer)
        ;
}

#endif //DSMCLIENT_PYTHON_H
