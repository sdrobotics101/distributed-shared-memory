#ifndef DSMSERVER_H
#define DSMSERVER_H

#include <iostream>
#include <cstdio>
#include <set>
#include <vector>
#include <unordered_map>
#include <atomic>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/array.hpp>
#include <boost/thread.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/sources/record_ostream.hpp>

#include "../Shared/DSMBase.h"

#define BASE_PORT 8888

using namespace boost::asio;

namespace dsm {
    class Server : public Base {
        public:
            Server(uint8_t portOffset);
            virtual ~Server();

            void start();
            void stop();
        private:
            void createLocalBuffer(std::string name, uint16_t size, uint16_t header, bool localOnly);
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
            void processData(const boost::system::error_code &error, size_t bytesReceived, std::string name, ip::udp::endpoint remoteEndpoint, ip::udp::socket* sock, ip::udp::endpoint* sender);

            void senderThreadFunction();
            void receiverThreadFunction();
            void handlerThreadFunction();
            std::atomic<bool> _isRunning;
            boost::thread* _senderThread;
            boost::thread* _receiverThread;
            boost::thread* _handlerThread;

            uint8_t _portOffset;
            ip::address _multicastAddress;
            uint16_t _multicastBasePort;
            uint8_t _multicastPortOffset;

            io_service _ioService;
            io_service::work _work;
            ip::udp::socket _senderSocket;
            ip::udp::socket _receiverSocket;
            ip::udp::endpoint _senderEndpoint;
            boost::array<char, 256> _receiveBuffer;

            std::vector<ip::udp::socket*> _sockets;
            std::vector<ip::udp::endpoint*> _senderEndpoints;
            std::unordered_map<std::string, boost::array<char, 256>> _remoteReceiveBuffers;

            //sorted sets of names of created local and remote buffers, so two with the same name aren't created
            //TODO need locks for these two
            std::set<std::string> _createdLocalBuffers;
            boost::shared_mutex _createdLocalBuffersMutex;

            std::set<std::string> _createdRemoteBuffers;
            boost::shared_mutex _createdRemoteBuffersMutex;

            //map from local buffer name to multicast endpoint of listeners
            //TODO? this really only needs to store the port, could create endpoints on the fly
            std::unordered_map<std::string, ip::udp::endpoint> _localBufferMulticastAddresses;
            boost::shared_mutex _localBufferMulticastAddressesMutex;

            //map from local buffer name to client IDs of listeners
            std::unordered_map<std::string, std::set<uint8_t>> _localBufferLocalListeners;

            //map from remote buffer to client IDs of local listeners
            std::unordered_map<std::string, std::set<uint8_t>> _remoteBufferLocalListeners;

            //list of remote buffers that we need an ACK for
            std::set<std::pair<std::string, ip::udp::endpoint>> _remoteBuffersToFetch;
            boost::shared_mutex _remoteBuffersToFetchMutex;

            //list of remote servers to ACK per buffer
            std::unordered_map<std::string, std::set<ip::udp::endpoint>> _remoteServersToACK;
            boost::shared_mutex _remoteServersToACKMutex;

            boost::log::sources::logger_mt _logger;
    };
}

#endif //DSMSERVER_H
