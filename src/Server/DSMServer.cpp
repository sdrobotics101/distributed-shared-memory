#include "DSMServer.h"

#ifdef LOGGING_ENABLED
std::ostream& operator<<(std::ostream& stream, severity_levels level)
{
    static const char* strings[] =
    {
        "periodic",
        "   trace",
        " startup",
        "teardown",
        "    info",
        "   error",
        "   debug"
    };
    if (level >= 0 && level < 7) {
        stream << strings[level];
    } else {
        stream << (int)(level);
    }
    return stream;
}
#endif

dsm::Server::Server(uint8_t serverID) : Base("server"+std::to_string(serverID)),
                                        _isRunning(false),
                                        _serverID(serverID),
                                        _multicastAddress(ip::address::from_string("239.255.0."+std::to_string(serverID))),
                                        _work(_ioService),
                                        _senderSocket(_ioService, ip::udp::v4()),
                                        _receiverSocket(_ioService, ip::udp::endpoint(ip::udp::v4(), RECEIVER_BASE_PORT+_serverID))
{
#ifdef LOGGING_ENABLED
    logging::formatter format = logging::expressions::stream <<
        "[" << severity << "] " <<
        logging::expressions::smessage;
    logging::core::get()->set_filter(severity > severity_levels::periodic);

    typedef logging::sinks::synchronous_sink<logging::sinks::text_ostream_backend> text_sink;
    boost::shared_ptr<text_sink> sink = boost::make_shared<text_sink>();
    boost::shared_ptr<std::ostream> stream(&std::clog, boost::null_deleter());
    sink->locked_backend()->add_stream(stream);
    sink->set_formatter(format);
    logging::core::get()->add_sink(sink);
#endif

    for (int i = 0; i < MAX_CLIENTS; i++) {
        _multicastPortOffsets[i] = 0;
    }

    LOG(_logger, startup) << "CONSTRUCTED SERVER";
}

//TODO is the ordering on this unsafe?
dsm::Server::~Server() {
    for (auto const &i : *_localBufferMap) {
        _segment.deallocate(_segment.get_address_from_handle(std::get<0>(i.second)));
        _segment.deallocate(std::get<2>(i.second).get());
    }
    for (auto const &i : *_remoteBufferMap) {
        _segment.deallocate(_segment.get_address_from_handle(std::get<0>(i.second)));
        _segment.deallocate(std::get<2>(i.second).get());
    }
    _segment.destroy<LocalBufferMap>("LocalBufferMap");
    _segment.destroy<RemoteBufferMap>("RemoteBufferMap");
    _segment.destroy<interprocess_sharable_mutex>("LocalBufferMapLock");
    _segment.destroy<interprocess_sharable_mutex>("RemoteBufferMapLock");

    /* clean up network */
    _isRunning = false;
    _ioService.stop();
    _senderThread->join();
    _receiverThread->join();
    _handlerThread->join();

    interprocess::message_queue::remove((_name+"_queue").c_str());
    interprocess::shared_memory_object::remove(_name.c_str());

    LOG(_logger, teardown) << "DESTROYED SERVER";
}

void dsm::Server::start() {
    //do some work to initialize network services, etc
    //create send and receive worker threads

    _isRunning = true;
    _senderThread.reset(new boost::thread(boost::bind(&Server::senderThreadFunction, this)));
    _receiverThread.reset(new boost::thread(boost::bind(&Server::receiverThreadFunction, this)));
    _handlerThread.reset(new boost::thread(boost::bind(&Server::handlerThreadFunction, this)));

    LOG(_logger, startup) << "MAIN LOOP START";

    unsigned int priority;
    interprocess::message_queue::size_type receivedSize;
    while (_isRunning.load()) {
        _messageQueue.receive(&_message, QUEUE_MESSAGE_SIZE, receivedSize, priority);

        if (receivedSize != QUEUE_MESSAGE_SIZE) {
            break;
        }

        switch(_message.options) {
            case CREATE_LOCAL:
                LOG(_logger, info) << "LOCAL: " << _message.name << " " << _message.footer.size << " " << (int)_message.clientID;
                createLocalBuffer(_message.name, _message.footer.size, _message.clientID, false);
                break;
            case CREATE_LOCALONLY:
                LOG(_logger, info) << "LOCALONLY: " << _message.name << " " << _message.footer.size << " " << (int)_message.clientID;
                createLocalBuffer(_message.name, _message.footer.size, _message.clientID, true);
                break;
            case FETCH_REMOTE:
                LOG(_logger, info) << "REMOTE: " << _message.name << " " << ip::address_v4(_message.footer.ipaddr) << " " << (int)_message.serverID;
                fetchRemoteBuffer(_message.name, _message.footer.ipaddr, _message.clientID, _message.serverID);
                break;
            case DISCONNECT_LOCAL:
                LOG(_logger, info) << "REMOVE LOCAL LISTENER: " << _message.name << " " << (int)_message.clientID;
                disconnectLocal(_message.name, _message.clientID);
                break;
            case DISCONNECT_REMOTE:
                LOG(_logger, info) << "REMOVE REMOTE LISTENER: " << _message.name << " " << (int)_message.clientID;
                disconnectRemote(_message.name, _message.footer.ipaddr, _message.clientID, _message.serverID);
                break;
            case DISCONNECT_CLIENT:
                LOG(_logger, info) << "CLIENT DISCONNECTED: " << (int)_message.clientID;
                disconnectClient(_message.clientID);
                break;
            default:
                LOG(_logger, severity_levels::error) << "UNKNOWN COMMAND";
                break;
        }
    }
    LOG(_logger, teardown) << "MAIN LOOP END";
}

void dsm::Server::stop() {
    LOG(_logger, teardown) << "SERVER STOPPING";
    _isRunning = false;
    uint8_t ignorePacket = -1;
    _senderSocket.send_to(asio::buffer(&ignorePacket, 1), ip::udp::endpoint(ip::address::from_string("127.0.0.1"), RECEIVER_BASE_PORT+_serverID));
    _messageQueue.send(0, 0, 0);
}

void dsm::Server::createLocalBuffer(LocalBufferKey key, uint16_t size, uint8_t clientID, bool localOnly) {
    LOG(_logger, trace) << "CREATING LOCAL BUFFER: " << key;
    interprocess::scoped_lock<interprocess_sharable_mutex> mapLock(*_localBufferMapLock);
    if (_localBufferMap->find(key) != _localBufferMap->end()) {
        _localBufferLocalListeners[key].insert(clientID);
        _clientSubscriptions[clientID].first.insert(key);
        return;
    } else {
        if (!localOnly && _multicastPortOffsets[clientID] >= MAX_BUFFERS_PER_CLIENT) {
            LOG(_logger, severity_levels::error) << "CLIENT " << (int)clientID << " HAS TOO MANY LOCAL BUFFERS";
            return;
        }
    }

    _localBufferLocalListeners[key].insert(clientID);
    _clientSubscriptions[clientID].first.insert(key);

    //TODO make these 2 allocate calls into one
    void* buf = _segment.allocate(size);
    interprocess::managed_shared_memory::handle_t handle = _segment.get_handle_from_address(buf);
    interprocess::offset_ptr<interprocess_sharable_mutex> mutex = static_cast<interprocess_sharable_mutex*>(_segment.allocate(sizeof(interprocess_sharable_mutex)));
    new (mutex.get()) interprocess_sharable_mutex;
    ip::udp::endpoint endpoint(_multicastAddress, 0);
    if (!localOnly) {
        endpoint.port(MULTICAST_BASE_PORT+(clientID * MAX_BUFFERS_PER_CLIENT)+_multicastPortOffsets[clientID]);
        _multicastPortOffsets[clientID]++;
    }
    _localBufferMap->insert(std::make_pair(key, std::make_tuple(handle, size, mutex, endpoint)));
}

void dsm::Server::createRemoteBuffer(RemoteBufferKey key, uint16_t size) {
    LOG(_logger, trace) << "CREATING REMOTE BUFFER: " << key;

    void* buf = _segment.allocate(size);
    interprocess::managed_shared_memory::handle_t handle = _segment.get_handle_from_address(buf);
    interprocess::offset_ptr<interprocess_sharable_mutex> mutex = static_cast<interprocess_sharable_mutex*>(_segment.allocate(sizeof(interprocess_sharable_mutex)));
    new (mutex.get()) interprocess_sharable_mutex;

    interprocess::scoped_lock<interprocess_sharable_mutex> mapLock(*_remoteBufferMapLock);
    _remoteBufferMap->insert(std::make_pair(key, std::make_tuple(handle, size, mutex)));
}

void dsm::Server::fetchRemoteBuffer(std::string name, uint32_t addr, uint8_t clientID, uint8_t serverID) {
    //TODO There must be a better way to translate the ipaddr
    ip::udp::endpoint endpoint(ip::address_v4(addr), RECEIVER_BASE_PORT+serverID);
    RemoteBufferKey key(name, endpoint);
    LOG(_logger, trace) << "FETCHING REMOTE BUFFER: " << key;
    _remoteBufferLocalListeners[key].insert(clientID);
    _clientSubscriptions[clientID].second.insert(key);

    interprocess::sharable_lock<interprocess_sharable_mutex> mapLock(*_remoteBufferMapLock);
    if (_remoteBufferMap->find(key) != _remoteBufferMap->end()) {
        return;
    }
    mapLock.unlock();

    boost::unique_lock<boost::shared_mutex> fetchLock(_remoteBuffersToFetchMutex);
    _remoteBuffersToFetch.insert(key);
}

void dsm::Server::disconnectLocal(std::string name, uint8_t clientID) {
    _localBufferLocalListeners[name].erase(clientID);
    _clientSubscriptions[clientID].first.erase(name);
    if (_localBufferLocalListeners[name].empty()) {
        removeLocalBuffer(name);
    }
}

void dsm::Server::disconnectRemote(std::string name, uint32_t addr, uint8_t clientID, uint8_t serverID) {
    RemoteBufferKey key(name, ip::udp::endpoint(ip::address_v4(addr), RECEIVER_BASE_PORT+serverID));
    _remoteBufferLocalListeners[key].erase(clientID);
    _clientSubscriptions[clientID].second.erase(key);
    if (_remoteBufferLocalListeners[key].empty()) {
        removeRemoteBuffer(key);
    }
}

void dsm::Server::disconnectClient(uint8_t clientID) {
    for (auto const &i : _clientSubscriptions[clientID].first) {
        _localBufferLocalListeners[i].erase(clientID);
        if (_localBufferLocalListeners[i].empty()) {
            removeLocalBuffer(i);
        }
    }
    for (auto const &i : _clientSubscriptions[clientID].second) {
        _remoteBufferLocalListeners[i].erase(clientID);
        if (_remoteBufferLocalListeners[i].empty()) {
            removeRemoteBuffer(i);
        }
    }
    _clientSubscriptions.erase(clientID);
    _multicastPortOffsets[clientID] = 0;
}

void dsm::Server::removeLocalBuffer(LocalBufferKey key) {
    LOG(_logger, trace) << "REMOVING LOCAL BUFFER " << key;
    interprocess::scoped_lock<interprocess_sharable_mutex> mapLock(*_localBufferMapLock);
    auto iterator = _localBufferMap->find(key);
    if (iterator == _localBufferMap->end()) {
        return;
    }
    _segment.deallocate(_segment.get_address_from_handle(std::get<0>(iterator->second)));
    _segment.deallocate(std::get<2>(iterator->second).get());
    _localBufferMap->erase(key);
}

void dsm::Server::removeRemoteBuffer(RemoteBufferKey key) {
    LOG(_logger, trace) << "REMOVING REMOTE BUFFER " << key;

    boost::unique_lock<boost::shared_mutex> fetchLock(_remoteBuffersToFetchMutex);
    auto fetchIterator = _remoteBuffersToFetch.find(key);
    if (fetchIterator != _remoteBuffersToFetch.end()) {
        _remoteBuffersToFetch.erase(key);
        return;
    }
    fetchLock.unlock();

    interprocess::scoped_lock<interprocess_sharable_mutex> mapLock(*_remoteBufferMapLock);
    auto mapIterator = _remoteBufferMap->find(key);
    if (mapIterator == _remoteBufferMap->end()) {
        return;
    }
    _segment.deallocate(_segment.get_address_from_handle(std::get<0>(mapIterator->second)));
    _segment.deallocate(std::get<2>(mapIterator->second).get());
    _remoteBufferMap->erase(key);
}

void dsm::Server::sendRequests() {
    boost::shared_lock<boost::shared_mutex> lock(_remoteBuffersToFetchMutex);
    for (auto const &i : _remoteBuffersToFetch) {
        LOG(_logger, trace) << "SENDING REQUEST " << i;
        boost::array<char, MAX_NAME_SIZE+3> sendBuffer;
        sendBuffer[0] = 0;  //so the server knows it's a request
        sendBuffer[1] = _serverID;    //so the server knows who to ACK
        sendBuffer[2] = i.name.length();
        std::strcpy(&sendBuffer[3], i.name.c_str());
        _senderSocket.async_send_to(asio::buffer(sendBuffer),
                                    i.endpoint,
                                    boost::bind(&dsm::Server::sendHandler,
                                                this,
                                                asio::placeholders::error,
                                                asio::placeholders::bytes_transferred));
    }
}

void dsm::Server::sendACKs() {
    boost::upgrade_lock<boost::shared_mutex> ackLock(_remoteServersToACKMutex);
    interprocess::sharable_lock<interprocess_sharable_mutex> mapLock(*_localBufferMapLock);
    for (auto &i : _remoteServersToACK) {
        auto iterator = _localBufferMap->find(i.first);
        if  (iterator == _localBufferMap->end()) {
            continue;
        }
        uint16_t multicastPort = std::get<3>(iterator->second).port();
        if (multicastPort == 0) {
            //TODO? specific code to tell remote that buffer is local only
            LOG(_logger, severity_levels::error) << "BUFFER " << i.first << " IS LOCAL ONLY";
            continue;
        }
        uint16_t len = std::get<1>(iterator->second);
        uint32_t multicastAddress = std::get<3>(iterator->second).address().to_v4().to_ulong();

        boost::array<char, MAX_NAME_SIZE+11> sendBuffer;
        sendBuffer[0] = 1; //so we know it's an ACK
        sendBuffer[1] = _serverID;
        sendBuffer[2] = i.first.length();
        //TODO array indices are correct but seem sketch
        memcpy(&sendBuffer[3], &len, sizeof(len));
        memcpy(&sendBuffer[5], &multicastAddress, sizeof(multicastAddress));
        memcpy(&sendBuffer[9], &multicastPort, sizeof(multicastPort));
        strcpy(&sendBuffer[11], i.first.c_str());
        for (auto const &j : i.second) {
            LOG(_logger, trace) << "SENDING ACK " << i.first << " TO " << j.address().to_v4().to_string() << " " << j.port();
            _senderSocket.async_send_to(asio::buffer(sendBuffer),
                                        j,
                                        boost::bind(&dsm::Server::sendHandler,
                                                    this,
                                                    asio::placeholders::error,
                                                    asio::placeholders::bytes_transferred));
        }
    }
    mapLock.unlock();

    boost::upgrade_to_unique_lock<boost::shared_mutex> uniqueLock(ackLock);
    _remoteServersToACK.clear();
}

void dsm::Server::sendData() {
    interprocess::sharable_lock<interprocess_sharable_mutex> mapLock(*_localBufferMapLock);
    for (auto const &i : *_localBufferMap) {
        LOG(_logger, periodic) << "SENDING DATA " << i.first << " TO " << std::get<3>(i.second).address() << " " << std::get<3>(i.second).port();
        ip::udp::endpoint endpoint = std::get<3>(i.second);
        if (endpoint.port() == 0) {
            continue;
        }
        void* data = _segment.get_address_from_handle(std::get<0>(i.second));
        uint16_t len = std::get<1>(i.second);
        interprocess::sharable_lock<interprocess_sharable_mutex> dataLock(*(std::get<2>(i.second).get()));
        _senderSocket.async_send_to(asio::buffer(data,len),
                                    endpoint,
                                    boost::bind(&dsm::Server::sendHandler,
                                                this,
                                                asio::placeholders::error,
                                                asio::placeholders::bytes_transferred));
    }
}

void dsm::Server::sendHandler(const boost::system::error_code&, std::size_t) {}

void dsm::Server::processRequest(ip::udp::endpoint remoteEndpoint) {
    std::string name(&_receiveBuffer[3], (uint8_t)_receiveBuffer[2]);
    remoteEndpoint.port(RECEIVER_BASE_PORT+(uint8_t)_receiveBuffer[1]);
    LOG(_logger, info) << "RECEIVED REQUEST FOR " << name << " FROM " << remoteEndpoint.address().to_string() << " " << remoteEndpoint.port();
    interprocess::sharable_lock<interprocess_sharable_mutex> mapLock(*_localBufferMapLock);
    if (_localBufferMap->find(name) != _localBufferMap->end()) {
        mapLock.unlock();
        boost::unique_lock<boost::shared_mutex> lock(_remoteServersToACKMutex);
        _remoteServersToACK[name].push_back(remoteEndpoint);
    }
#ifdef LOGGING_ENABLED
    else {
        LOG(_logger, severity_levels::error) << "COULDN'T FIND BUFFER " << name;
    }
#endif
}

void dsm::Server::processACK(ip::udp::endpoint remoteEndpoint) {
    std::string name(&_receiveBuffer[11], _receiveBuffer[2]);
    remoteEndpoint.port(RECEIVER_BASE_PORT+(uint8_t)_receiveBuffer[1]);
    RemoteBufferKey key(name, remoteEndpoint);
    {
        //check if <name, addr, port> exists in remotes to create
        LOG(_logger, info) << "RECEIVED ACK FOR " << key;
        if (_remoteBuffersToFetch.find(key) == _remoteBuffersToFetch.end()) {
            return;
        }
        //delete entry if true and continue
        boost::unique_lock<boost::shared_mutex> lock(_remoteBuffersToFetchMutex);
        _remoteBuffersToFetch.erase(key);
    }
    {
        //get the buffer length and create it
        uint16_t buflen;
        memcpy(&buflen, &_receiveBuffer[3], sizeof(uint16_t));
        createRemoteBuffer(key, buflen);
        _remoteReceiveBuffers.insert(std::make_pair(key, std::make_pair(boost::shared_array<char>(new char[buflen]), buflen)));
    }
    {
        //create socket and start handler
        uint32_t mcastaddr;
        memcpy(&mcastaddr, &_receiveBuffer[5], sizeof(mcastaddr));
        uint16_t mcastport;
        memcpy(&mcastport, &_receiveBuffer[9], sizeof(mcastport));

        boost::shared_ptr<ip::udp::socket> sock(new ip::udp::socket(_ioService));
        ip::udp::endpoint listenEndpoint(ip::address_v4::from_string("0.0.0.0"), mcastport);
        sock->open(ip::udp::v4());
        sock->set_option(ip::udp::socket::reuse_address(true));
        sock->bind(listenEndpoint);
        sock->set_option(ip::multicast::join_group(ip::address_v4(mcastaddr)));
        ip::udp::endpoint senderEndpoint;
        sock->async_receive_from(asio::buffer(_remoteReceiveBuffers[key].first.get(), _remoteReceiveBuffers[key].second),
                                 senderEndpoint,
                                 boost::bind(&dsm::Server::processData,
                                             this,
                                             asio::placeholders::error,
                                             asio::placeholders::bytes_transferred,
                                             key,
                                             sock.get(),
                                             senderEndpoint));
        _sockets.push_back(sock);
    }
}

void dsm::Server::processData(const boost::system::error_code &error, size_t bytesReceived, RemoteBufferKey key, ip::udp::socket* sock, ip::udp::endpoint sender) {
    LOG(_logger, periodic) << "RECEIVED DATA FOR REMOTE " << key;
    if (error) {
        LOG(_logger, severity_levels::error) << "ERROR PROCESSING DATA FOR " << key;
        return;
    }
    interprocess::sharable_lock<interprocess_sharable_mutex> mapLock(*_remoteBufferMapLock);
    auto iterator = _remoteBufferMap->find(key);
    if (iterator == _remoteBufferMap->end()) {
        LOG(_logger, severity_levels::error) << "MAP ENTRY FOR " << key << " DOESN'T EXIST";
        _remoteReceiveBuffers.erase(key);
        return;
    }
    uint16_t len = std::get<1>(iterator->second);
    if (len != bytesReceived) {
        LOG(_logger, severity_levels::error) << "RECEIVED " << bytesReceived << " BYTES WHEN " << len << " WERE EXPECTED";
        return;
    }
    void* ptr = _segment.get_address_from_handle(std::get<0>(iterator->second));
    interprocess::scoped_lock<interprocess_sharable_mutex> dataLock(*(std::get<2>(iterator->second).get()));
    memcpy(ptr, _remoteReceiveBuffers[key].first.get(), len);
    sock->async_receive_from(asio::buffer(_remoteReceiveBuffers[key].first.get(), _remoteReceiveBuffers[key].second),
                             sender,
                             boost::bind(&dsm::Server::processData,
                                         this,
                                         asio::placeholders::error,
                                         asio::placeholders::bytes_transferred,
                                         key,
                                         sock,
                                         sender));
}

void dsm::Server::senderThreadFunction() {
    LOG(_logger, startup) << "SENDER START";
    while (_isRunning.load()) {
        LOG(_logger, periodic) << "SENDER";
        sendRequests();
        sendACKs();
        sendData();
        boost::this_thread::sleep_for(boost::chrono::milliseconds(SENDER_DELAY));
    }
    LOG(_logger, teardown) << "SENDER END";
}

void dsm::Server::receiverThreadFunction() {
    LOG(_logger, startup) << "RECEIVER START";
    boost::system::error_code err;
    ip::udp::endpoint remoteEndpoint;
    while (_isRunning.load()) {
        _receiverSocket.receive_from(asio::buffer(_receiveBuffer), remoteEndpoint, 0, err);

        LOG(_logger, periodic) << "RECEIVER GOT PACKET";

        switch (_receiveBuffer[0]) {
            case 0:
                processRequest(remoteEndpoint);
                break;
            case 1:
                processACK(remoteEndpoint);
                break;
            default:
            LOG(_logger, info) << "IGNORING PACKET";
            continue;
        }
    }
    LOG(_logger, teardown) << "RECEIVER END";
}

void dsm::Server::handlerThreadFunction() {
    LOG(_logger, startup) << "HANDLE RECEIVE START";
    _ioService.run();
    LOG(_logger, teardown) << "HANDLE RECEIVE END";
}
