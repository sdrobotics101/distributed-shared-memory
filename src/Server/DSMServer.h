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

#include <boost/log/core.hpp>
#include <boost/core/null_deleter.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>

#include "../Shared/DSMBase.h"

#define REQUEST_BASE_PORT 8888
#define MULTICAST_BASE_PORT 30000

#define MAX_CLIENTS 16
#define MAX_BUFFERS_PER_CLIENT 64

#define SENDER_DELAY 10

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace logging = boost::log;

enum severity_levels {
    trace,
    startup,
    teardown,
    info,
    error,
    debug
};

BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", severity_levels);

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
            void disconnectClient(uint16_t header);

            void removeLocalBuffer(std::string name);
            void removeRemoteBuffer(std::string name, std::string ipaddr);

            void sendRequests();
            void sendACKs();
            void sendData();
            void sendHandler(const boost::system::error_code&, std::size_t);

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
            uint8_t _multicastPortOffsets[MAX_CLIENTS];

            asio::io_service _ioService;
            asio::io_service::work _work;
            ip::udp::socket _senderSocket;
            ip::udp::socket _receiverSocket;
            ip::udp::endpoint _senderEndpoint;
            boost::array<char, 36> _receiveBuffer;

            std::vector<ip::udp::socket*> _sockets;
            std::vector<ip::udp::endpoint*> _senderEndpoints;
            std::unordered_map<std::string, boost::array<char, 256>> _remoteReceiveBuffers;

            //sorted sets of names of created local and remote buffers, so two with the same name aren't created
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

            //map from client ID to list of local and remote buffers subscribed to
            std::unordered_map<uint8_t, std::pair<std::set<std::string>, std::set<std::pair<std::string, std::string>>>> _clientSubscriptions;

            logging::sources::severity_logger_mt<severity_levels> _logger;
    };
}

#endif //DSMSERVER_H
