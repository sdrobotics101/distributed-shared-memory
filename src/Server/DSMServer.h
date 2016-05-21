#ifndef DSMSERVER_H
#define DSMSERVER_H

#include <iostream>
#include <cstdio>
#include <set>
#include <unordered_map>
#include <thread>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/array.hpp>
#include <boost/thread.hpp>

#include "../Shared/DSMBase.h"

#define BASE_PORT 8888

using namespace boost::asio;

namespace dsm {
    class Server : public Base {
        public:
            Server(std::string name, uint8_t portOffset);
            virtual ~Server();

            void start();
        private:
            void allocateLocalBuffer(std::string name, uint16_t size);
            /* void allocateRemoteBuffers(); */

            void removeLocalBuffer(std::string name);

            void senderThreadFunction();
            void receieverThreadFunction();
            bool _isRunning;
            std::thread *_senderThread;
            std::thread *_receiverThread;

            uint8_t _portOffset;

            io_service _ioService;
            ip::udp::socket* _socket;
            ip::udp::endpoint _endpoint;
            boost::array<char, 256> _receiveBuffer;

            //sorted sets of names of created local and remote buffers, so two with the same name aren't created
            std::set<std::string> _createdLocalBuffers;
            std::set<std::string> _createdRemoteBuffers;

            //map from local buffer name to (ip address, port offset) of listeners
            std::unordered_map<std::string, std::set<std::pair<uint8_t[4], uint8_t>>> _localBufferNetworkListeners;

            //map from local buffer name to client IDs of listeners
            std::unordered_map<std::string, std::set<uint8_t>> _localBufferLocalListeners;

            //map from remote buffer name to client IDs of listeners
            std::unordered_map<std::string, std::set<uint8_t>> _remoteBufferLocalListeners;
    };
}

#endif //DSMSERVER_H
