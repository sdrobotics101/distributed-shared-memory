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
#include "../Dependencies/Log/src/Log.h"
#else
#include "../Dependencies/Log/src/LogDisabled.h"
#endif

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/interprocess_sharable_mutex.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>

#include "../Shared/DSMDefinitions.h"
#include "../Shared/DSMBase.h"

namespace dsm {
    class Server : public Base {
        public:
            Server(uint8_t serverID);
            virtual ~Server();

            void start();
            void stop();
        private:
            void createLocalBuffer(LocalBufferKey key, uint16_t size, uint8_t clientID, bool localOnly);
            void createRemoteBuffer(RemoteBufferKey key, uint16_t size);
            void fetchRemoteBuffer(BufferName name, uint32_t addr, uint8_t clientID, uint8_t serverID);

            void disconnectLocal(BufferName name, uint8_t clientID);
            void disconnectRemote(BufferName name, uint32_t addr, uint8_t clientID, uint8_t serverID);
            void disconnectClient(uint8_t clientID);

            void removeLocalBuffer(LocalBufferKey key);
            void removeRemoteBuffer(RemoteBufferKey key);

            void sendRequests();
            void sendACKs();
            void sendData();
            void sendHandler(const boost::system::error_code&, std::size_t);

            void processRequest(ip::udp::endpoint remoteEndpoint);
            void processACK(ip::udp::endpoint remoteEndpoint, bool localOnly);
            void processData(const boost::system::error_code &error, size_t bytesReceived, RemoteBufferKey key, boost::shared_ptr<ip::udp::socket> sock, ip::udp::endpoint sender, boost::shared_ptr<asio::deadline_timer> timer);
            void setBufferToInactive(const boost::system::error_code &error, RemoteBufferKey key);

            void senderThreadFunction();
            void receiverThreadFunction();
            void handlerThreadFunction();
            std::atomic<bool> _isRunning;
            boost::scoped_ptr<boost::thread> _senderThread;
            boost::scoped_ptr<boost::thread> _receiverThread;
            boost::scoped_ptr<boost::thread> _handlerThread;

            uint8_t _serverID;
            ip::address _multicastAddress;
            uint8_t _multicastPortOffsets[MAX_CLIENTS];

            asio::io_service _ioService;
            asio::io_service::work _work;
            ip::udp::socket _senderSocket;
            ip::udp::socket _receiverSocket;
            ip::udp::endpoint _senderEndpoint;
            boost::array<char, 36> _receiveBuffer;

            boost::unordered_map<RemoteBufferKey, std::pair<boost::shared_array<char>, uint16_t>> _remoteReceiveBuffers;

            //map from local buffer name to client IDs of listeners
            boost::unordered_map<LocalBufferKey, boost::unordered_set<uint8_t>> _localBufferLocalListeners;

            //map from remote buffer to client IDs of local listeners
            boost::unordered_map<RemoteBufferKey, boost::unordered_set<uint8_t>> _remoteBufferLocalListeners;

            //map from client ID to list of local and remote buffers subscribed to
            boost::unordered_map<uint8_t, std::pair<boost::unordered_set<LocalBufferKey>, boost::unordered_set<RemoteBufferKey>>> _clientSubscriptions;

            //list of remote buffers that we need an ACK for
            boost::unordered_set<RemoteBufferKey> _remoteBuffersToFetch;
            boost::shared_mutex _remoteBuffersToFetchMutex;

            //list of remote servers to ACK per buffer
            boost::unordered_map<LocalBufferKey, std::vector<ip::udp::endpoint>> _remoteServersToACK;
            boost::shared_mutex _remoteServersToACKMutex;

#ifdef LOGGING_ENABLED
            logging::sources::severity_logger_mt<logger::severityLevel> _logger;
#endif
    };
}

#endif //DSMSERVER_H
