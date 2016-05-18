#ifndef DSMSERVER_H
#define DSMSERVER_H

#include <iostream>
#include <set>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/array.hpp>
#include <boost/thread.hpp>

#include "../Shared/DSMBase.h"

using namespace boost::asio;

class DSMServer : public DSMBase {
    public:
        DSMServer(std::string name, int port);
        virtual ~DSMServer();

        void start();

        void dump();
    private:
        void allocateLocalBuffers();
        /* void allocateRemoteBuffers(); */

        void startReceive();

        void handleReceive(const boost::system::error_code& error, std::size_t bytesTransferred);
        /* void handleSend(); */

        io_service _ioService;
        ip::udp::socket _socket;
        ip::udp::endpoint _endpoint;
        boost::array<char, 256> _receiveBuffer;

        std::set<std::string> _createdLocalBuffers;
        std::set<std::string> _createdRemoteBuffers;
};

#endif //DSMSERVER_H
