#ifndef DSMSERVER_H
#define DSMSERVER_H

#include <iostream>
#include <cstdint>
#include <vector>
#include <atomic>

#include <boost/bind.hpp>
#include <boost/array.hpp>
#include <boost/thread.hpp>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>

#ifdef LOGGING_ENABLED
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/core/null_deleter.hpp>
#endif

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/interprocess_sharable_mutex.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>

#include "../Shared/DSMDefinitions.h"
#include "../Shared/DSMBase.h"

#define MULTICAST_BASE_PORT 30000

#define MAX_CLIENTS 128
#define MAX_BUFFERS_PER_CLIENT 8

#define SENDER_DELAY 10 //milliseconds

#ifdef LOGGING_ENABLED
namespace logging = boost::log;

enum severity_levels {
    periodic,
    trace,
    startup,
    teardown,
    info,
    error,
    debug
};

BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", severity_levels);
#endif

namespace dsm {
    class Server : public Base {
        public:
            Server(uint8_t portOffset);
            virtual ~Server();

            void start();
            void stop();
        private:
            void createLocalBuffer(LocalBufferKey key, uint16_t size, uint8_t clientID, bool localOnly);
            void createRemoteBuffer(RemoteBufferKey key, uint16_t size);
            void fetchRemoteBuffer(std::string name, uint32_t addr, uint8_t clientID, uint8_t options);

            void disconnectLocal(std::string name, uint8_t clientID);
            void disconnectRemote(std::string name, uint32_t addr, uint8_t clientID, uint8_t options);
            void disconnectClient(uint8_t clientID);

            void removeLocalBuffer(LocalBufferKey key);
            void removeRemoteBuffer(RemoteBufferKey key);

            void sendRequests();
            void sendACKs();
            void sendData();
            void sendHandler(const boost::system::error_code&, std::size_t);

            void processRequest(ip::udp::endpoint remoteEndpoint);
            void processACK(ip::udp::endpoint remoteEndpoint);
            void processData(const boost::system::error_code &error, size_t bytesReceived, RemoteBufferKey key, ip::udp::socket* sock, ip::udp::endpoint sender);

            void senderThreadFunction();
            void receiverThreadFunction();
            void handlerThreadFunction();
            std::atomic<bool> _isRunning;
            boost::scoped_ptr<boost::thread> _senderThread;
            boost::scoped_ptr<boost::thread> _receiverThread;
            boost::scoped_ptr<boost::thread> _handlerThread;

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

            std::vector<boost::shared_ptr<ip::udp::socket>> _sockets;
            boost::unordered_map<RemoteBufferKey, std::pair<boost::shared_array<char>, uint16_t>> _remoteReceiveBuffers;

            //map from local buffer name to client IDs of listeners
            boost::unordered_map<LocalBufferKey, std::set<uint8_t>> _localBufferLocalListeners;

            //map from remote buffer to client IDs of local listeners
            boost::unordered_map<RemoteBufferKey, std::set<uint8_t>> _remoteBufferLocalListeners;

            //map from client ID to list of local and remote buffers subscribed to
            boost::unordered_map<uint8_t, std::pair<boost::unordered_set<LocalBufferKey>, boost::unordered_set<RemoteBufferKey>>> _clientSubscriptions;

            //list of remote buffers that we need an ACK for
            boost::unordered_set<RemoteBufferKey> _remoteBuffersToFetch;
            boost::shared_mutex _remoteBuffersToFetchMutex;

            //list of remote servers to ACK per buffer
            boost::unordered_map<LocalBufferKey, std::vector<ip::udp::endpoint>> _remoteServersToACK;
            boost::shared_mutex _remoteServersToACKMutex;

#ifdef LOGGING_ENABLED
            logging::sources::severity_logger_mt<severity_levels> _logger;
#endif
    };
}

#endif //DSMSERVER_H
