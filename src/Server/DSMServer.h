#ifndef DSMSERVER_H
#define DSMSERVER_H

#include <iostream>
#include <cstdio>
#include <set>
#include <unordered_map>
#include <atomic>

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
            Server(std::string name, uint8_t portOffset, std::string multicastAddress);
            virtual ~Server();

            void start();
            void stop();
        private:
            void createLocalBuffer(std::string name, uint16_t size, uint16_t header);
            void createRemoteBuffer(std::string name, struct in_addr addr, uint16_t header);
            void disconnectLocal(std::string name, uint16_t header);

            void removeLocalBuffer(std::string name);

            void senderThreadFunction();
            void receiverThreadFunction();
            std::atomic<bool> _isRunning;
            boost::thread* _senderThread;
            boost::thread* _receiverThread;

            uint8_t _portOffset;
            std::string _multicastAddress;

            io_service* _ioService;
            ip::udp::socket* _senderSocket;
            ip::udp::socket* _receiverSocket;
            boost::array<char, 256> _receiveBuffer;

            //sorted sets of names of created local and remote buffers, so two with the same name aren't created
            std::set<std::string> _createdLocalBuffers;
            std::set<std::string> _createdRemoteBuffers;

            //map from local buffer name to multicast endpoint of listeners
            std::unordered_map<std::string, ip::udp::endpoint> _localBufferMulticastAddresses;

            //map from local buffer name to client IDs of listeners
            std::unordered_map<std::string, std::set<uint8_t>> _localBufferLocalListeners;

            //list of remote buffers that we need an ACK for
            std::set<std::pair<std::string, ip::udp::endpoint*>> _remoteBuffersToCreate;
    };
}

#endif //DSMSERVER_H
