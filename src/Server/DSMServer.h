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
            Server(std::string name, uint8_t portOffset, std::string multicastAddress, uint16_t multicastBasePort);
            virtual ~Server();

            void start();
            void stop();
        private:
            void createLocalBuffer(std::string name, uint16_t size, uint16_t header);
            void createRemoteBuffer(std::string name, std::string ipaddr, uint16_t size);
            void fetchRemoteBuffer(std::string name, struct in_addr addr, uint16_t header);

            void disconnectLocal(std::string name, uint16_t header);
            void disconnectRemote(std::string name, struct in_addr addr, uint16_t header);

            void removeLocalBuffer(std::string name);
            void removeRemoteBuffer(std::string name, std::string ipaddr);

            void sendRequests();
            void sendACKs();
            void sendData();

            void processRequest(ip::udp::endpoint remoteEndpoint);
            void processACK(ip::udp::endpoint remoteEndpoint);

            void senderThreadFunction();
            void receiverThreadFunction();
            std::atomic<bool> _isRunning;
            boost::thread* _senderThread;
            boost::thread* _receiverThread;

            uint8_t _portOffset;
            ip::address _multicastAddress;
            uint16_t _multicastBasePort;
            uint8_t _multicastPortOffset;

            io_service* _ioService;
            ip::udp::socket* _senderSocket;
            ip::udp::socket* _receiverSocket;
            boost::array<char, 256> _receiveBuffer;

            //sorted sets of names of created local and remote buffers, so two with the same name aren't created
            std::set<std::string> _createdLocalBuffers;
            std::set<std::string> _createdRemoteBuffers;

            //map from local buffer name to multicast endpoint of listeners
            std::unordered_map<std::string, ip::udp::endpoint> _localBufferMulticastAddresses;
            boost::shared_mutex _localBufferMulticastAddressesMutex;

            //map from local buffer name to client IDs of listeners
            std::unordered_map<std::string, std::set<uint8_t>> _localBufferLocalListeners;

            //map from remote buffer to client IDs of local listeners
            std::unordered_map<std::string, std::set<uint8_t>> _remoteBufferLocalListeners;

            //list of remote buffers that we need an ACK for
            std::set<std::pair<std::string, ip::udp::endpoint>> _remoteBuffersToCreate;
            boost::shared_mutex _remoteBuffersToCreateMutex;

            //list of remote servers to ack per buffer
            std::unordered_map<std::string, std::set<ip::udp::endpoint>> _remoteServersToACK;
            boost::shared_mutex _remoteServersToACKMutex;
    };
}

#endif //DSMSERVER_H
