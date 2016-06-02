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

dsm::Server::Server(uint8_t portOffset) : Base("server"+std::to_string((portOffset < 0 || portOffset > 15) ? 0 : portOffset)),
                                          _isRunning(false),
                                          _portOffset((portOffset < 0 || portOffset > 15) ? 0 : portOffset),
                                          _multicastAddress(ip::address::from_string("239.255.0."+std::to_string(portOffset+1))),
                                          _multicastBasePort(MULTICAST_BASE_PORT+(portOffset*MAX_CLIENTS*MAX_BUFFERS_PER_CLIENT)),
                                          _work(_ioService),
                                          _senderSocket(_ioService, ip::udp::v4()),
                                          _receiverSocket(_ioService, ip::udp::endpoint(ip::udp::v4(), REQUEST_BASE_PORT+_portOffset))
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

#ifdef LOGGING_ENABLED
    BOOST_LOG_SEV(_logger, startup) << "CONSTRUCTED SERVER";
#endif
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

#ifdef LOGGING_ENABLED
    BOOST_LOG_SEV(_logger, teardown) << "DESTROYED SERVER";
#endif
}

void dsm::Server::start() {
    //do some work to initialize network services, etc
    //create send and receive worker threads

    _isRunning = true;
    _senderThread.reset(new boost::thread(boost::bind(&Server::senderThreadFunction, this)));
    _receiverThread.reset(new boost::thread(boost::bind(&Server::receiverThreadFunction, this)));
    _handlerThread.reset(new boost::thread(boost::bind(&Server::handlerThreadFunction, this)));

#ifdef LOGGING_ENABLED
    BOOST_LOG_SEV(_logger, startup) << "MAIN LOOP START";
#endif

    unsigned int priority;
    interprocess::message_queue::size_type receivedSize;
    while (_isRunning.load()) {
        _messageQueue.receive(&_message, MESSAGE_SIZE, receivedSize, priority);

        if (receivedSize != 32) {
            break;
        }

        switch((_message.header & 0xF0) >> 4) {
            case CREATE_LOCAL:
#ifdef LOGGING_ENABLED
                BOOST_LOG_SEV(_logger, info) << "LOCAL: " << _message.name << " " << _message.footer.size << " " << (_message.header & 0x0F);
#endif
                createLocalBuffer(_message.name, _message.footer.size, _message.header, false);
                break;
            case CREATE_LOCALONLY:
#ifdef LOGGING_ENABLED
                BOOST_LOG_SEV(_logger, info) << "LOCALONLY: " << _message.name << " " << _message.footer.size << " " << (_message.header & 0x0F);
#endif
                createLocalBuffer(_message.name, _message.footer.size, _message.header, true);
                break;
            case FETCH_REMOTE:
#ifdef LOGGING_ENABLED
                BOOST_LOG_SEV(_logger, info) << "REMOTE: " << _message.name << " " << inet_ntoa(_message.footer.ipaddr) << " " << ((_message.header >> 8) & 0x0F);
#endif
                fetchRemoteBuffer(_message.name, _message.footer.ipaddr, _message.header);
                break;
            case DISCONNECT_LOCAL:
#ifdef LOGGING_ENABLED
                BOOST_LOG_SEV(_logger, info) << "REMOVE LOCAL LISTENER: " << _message.name << " " << (_message.header & 0x0F);
#endif
                disconnectLocal(_message.name, _message.header);
                break;
            case DISCONNECT_REMOTE:
#ifdef LOGGING_ENABLED
                BOOST_LOG_SEV(_logger, info) << "REMOVE REMOTE LISTENER: " << _message.name << " " << (_message.header & 0x0F);
#endif
                disconnectRemote(_message.name, _message.footer.ipaddr, _message.header);
                break;
            case DISCONNECT_CLIENT:
#ifdef LOGGING_ENABLED
                BOOST_LOG_SEV(_logger, info) << "CLIENT DISCONNECTED: " << (_message.header & 0x0F);
#endif
                disconnectClient(_message.header);
                break;
            default:
#ifdef LOGGING_ENABLED
                BOOST_LOG_SEV(_logger, severity_levels::error) << "UNKNOWN COMMAND";
#endif
                break;
        }
        _message.reset();
    }
#ifdef LOGGING_ENABLED
    BOOST_LOG_SEV(_logger, teardown) << "MAIN LOOP END";
#endif
}

void dsm::Server::stop() {
#ifdef LOGGING_ENABLED
    BOOST_LOG_SEV(_logger, teardown) << "SERVER STOPPING";
#endif
    _isRunning = false;
    uint8_t ignorePacket = -1;
    _senderSocket.send_to(asio::buffer(&ignorePacket, 1), ip::udp::endpoint(ip::address::from_string("127.0.0.1"), REQUEST_BASE_PORT+_portOffset));
    _messageQueue.send(0, 0, 0);
}

void dsm::Server::createLocalBuffer(LocalBufferKey key, uint16_t size, uint16_t header, bool localOnly) {
#ifdef LOGGING_ENABLED
    BOOST_LOG_SEV(_logger, trace) << "CREATING LOCAL BUFFER: " << key;
#endif
    uint8_t clientID = header & 0x0F;
    interprocess::scoped_lock<interprocess_sharable_mutex> mapLock(*_localBufferMapLock);
    if (_localBufferMap->find(key) != _localBufferMap->end()) {
        _localBufferLocalListeners[key].insert(clientID);
        _clientSubscriptions[clientID].first.insert(key);
        return;
    } else {
        if (_multicastPortOffsets[clientID] > MAX_BUFFERS_PER_CLIENT-1) {
#ifdef LOGGING_ENABLED
            BOOST_LOG_SEV(_logger, severity_levels::error) << "CLIENT " << clientID << " HAS TOO MANY LOCAL BUFFERS";
#endif
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
    ip::udp::endpoint endpoint;
    if (localOnly) {
        endpoint = ip::udp::endpoint(_multicastAddress, 0);
    } else {
        endpoint = ip::udp::endpoint(_multicastAddress, _multicastBasePort+(clientID * MAX_BUFFERS_PER_CLIENT)+_multicastPortOffsets[clientID]);
        _multicastPortOffsets[clientID]++;
    }
    _localBufferMap->insert(std::make_pair(key, std::make_tuple(handle, size, mutex, endpoint)));
}

void dsm::Server::createRemoteBuffer(RemoteBufferKey key, uint16_t size) {
#ifdef LOGGING_ENABLED
    BOOST_LOG_SEV(_logger, trace) << "CREATING REMOTE BUFFER: " << key;
#endif

    void* buf = _segment.allocate(size);
    interprocess::managed_shared_memory::handle_t handle = _segment.get_handle_from_address(buf);
    interprocess::offset_ptr<interprocess_sharable_mutex> mutex = static_cast<interprocess_sharable_mutex*>(_segment.allocate(sizeof(interprocess_sharable_mutex)));
    new (mutex.get()) interprocess_sharable_mutex;

    interprocess::scoped_lock<interprocess_sharable_mutex> mapLock(*_remoteBufferMapLock);
    _remoteBufferMap->insert(std::make_pair(key, std::make_tuple(handle, size, mutex)));
}

void dsm::Server::fetchRemoteBuffer(std::string name, struct in_addr addr, uint16_t header) {
    uint8_t clientID = header & 0x0F;
    //TODO There must be a better way to translate the ipaddr
    std::string ipaddr = inet_ntoa(addr);
    uint8_t portOffset = (header >> 8) & 0x0F;
    ip::udp::endpoint endpoint(ip::address::from_string(ipaddr), REQUEST_BASE_PORT+portOffset);
    RemoteBufferKey key(name, endpoint);
#ifdef LOGGING_ENABLED
    BOOST_LOG_SEV(_logger, trace) << "FETCHING REMOTE BUFFER: " << key;
#endif
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

void dsm::Server::disconnectLocal(std::string name, uint16_t header) {
    uint8_t clientID = header & 0x0F;
    _localBufferLocalListeners[name].erase(clientID);
    _clientSubscriptions[clientID].first.erase(name);
    if (_localBufferLocalListeners[name].empty()) {
        removeLocalBuffer(name);
    }
}

void dsm::Server::disconnectRemote(std::string name, struct in_addr addr, uint16_t header) {
    std::string ipaddr = inet_ntoa(addr);
    uint8_t clientID = header & 0x0F;
    uint8_t portOffset = (header >> 8) & 0x0F;
    RemoteBufferKey key(name, ip::udp::endpoint(ip::address::from_string(ipaddr), REQUEST_BASE_PORT+portOffset));
    _remoteBufferLocalListeners[key].erase(clientID);
    _clientSubscriptions[clientID].second.erase(key);
    if (_remoteBufferLocalListeners[key].empty()) {
        removeRemoteBuffer(key);
    }
}

void dsm::Server::disconnectClient(uint16_t header) {
    uint8_t clientID = (header & 0x0F);
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
}

void dsm::Server::removeLocalBuffer(LocalBufferKey key) {
#ifdef LOGGING_ENABLED
    BOOST_LOG_SEV(_logger, trace) << "REMOVING LOCAL BUFFER " << key;
#endif
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
#ifdef LOGGING_ENABLED
    BOOST_LOG_SEV(_logger, trace) << "REMOVING REMOTE BUFFER " << key;
#endif

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
#ifdef LOGGING_ENABLED
        BOOST_LOG_SEV(_logger, trace) << "SENDING REQUEST " << i;
#endif
        boost::array<char, 28> sendBuffer;
        sendBuffer[0] = _portOffset;    //so the server knows who to ACK
        sendBuffer[1] = i.name.length();
        std::strcpy(&sendBuffer[2], i.name.c_str());
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
#ifdef LOGGING_ENABLED
            BOOST_LOG_SEV(_logger, severity_levels::error) << "BUFFER " << i.first << " IS LOCAL ONLY";
#endif
            continue;
        }
        uint16_t len = std::get<1>(iterator->second);
        uint32_t multicastAddress = std::get<3>(iterator->second).address().to_v4().to_ulong();

        boost::array<char, 36> sendBuffer;
        sendBuffer[0] = _portOffset;
        sendBuffer[0] |= 0x80;  //so we know it's an ACK
        sendBuffer[1] = i.first.length();
        //TODO array indices are correct but seem sketch
        memcpy(&sendBuffer[2], &len, sizeof(len));
        memcpy(&sendBuffer[4], &multicastAddress, sizeof(multicastAddress));
        memcpy(&sendBuffer[8], &multicastPort, sizeof(multicastPort));
        strcpy(&sendBuffer[10], i.first.c_str());
        for (auto const &j : i.second) {
#ifdef LOGGING_ENABLED
            BOOST_LOG_SEV(_logger, trace) << "SENDING ACK " << i.first << " TO " << j.address().to_v4().to_string() << " " << j.port();
#endif
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
#ifdef LOGGING_ENABLED
        BOOST_LOG_SEV(_logger, periodic) << "SENDING DATA " << i.first << " TO " << std::get<3>(i.second).address() << " " << std::get<3>(i.second).port();
#endif
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
    std::string name(&_receiveBuffer[2], (uint8_t)_receiveBuffer[1]);
    remoteEndpoint.port(REQUEST_BASE_PORT+_receiveBuffer[0]);
#ifdef LOGGING_ENABLED
    BOOST_LOG_SEV(_logger, info) << "RECEIVED REQUEST FOR " << name << " FROM " << remoteEndpoint.address().to_string() << " " << remoteEndpoint.port();
#endif
    interprocess::sharable_lock<interprocess_sharable_mutex> mapLock(*_localBufferMapLock);
    if (_localBufferMap->find(name) != _localBufferMap->end()) {
        mapLock.unlock();
        boost::unique_lock<boost::shared_mutex> lock(_remoteServersToACKMutex);
        _remoteServersToACK[name].push_back(remoteEndpoint);
    }
#ifdef LOGGING_ENABLED
    else {
        BOOST_LOG_SEV(_logger, severity_levels::error) << "COULDN'T FIND BUFFER " << name;
    }
#endif
}

void dsm::Server::processACK(ip::udp::endpoint remoteEndpoint) {
    std::string name(&_receiveBuffer[10], _receiveBuffer[1]);
    remoteEndpoint.port(REQUEST_BASE_PORT+(_receiveBuffer[0] & 0x0F));
    RemoteBufferKey key(name, remoteEndpoint);
    {
        //check if <name, addr, port> exists in remotes to create
#ifdef LOGGING_ENABLED
        BOOST_LOG_SEV(_logger, info) << "RECEIVED ACK FOR " << key;
#endif
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
        memcpy(&buflen, &_receiveBuffer[2], sizeof(uint16_t));
        createRemoteBuffer(key, buflen);
        _remoteReceiveBuffers.insert(std::make_pair(key, std::make_pair(boost::shared_array<char>(new char[buflen]), buflen)));
    }
    {
        //create socket and start handler
        uint32_t mcastaddr;
        memcpy(&mcastaddr, &_receiveBuffer[4], sizeof(mcastaddr));
        uint16_t mcastport;
        memcpy(&mcastport, &_receiveBuffer[8], sizeof(mcastport));

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
#ifdef LOGGING_ENABLED
    BOOST_LOG_SEV(_logger, periodic) << "RECEIVED DATA FOR REMOTE " << key;
#endif
    if (error) {
#ifdef LOGGING_ENABLED
        BOOST_LOG_SEV(_logger, severity_levels::error) << "ERROR PROCESSING DATA FOR " << key;
#endif
        return;
    }
    interprocess::sharable_lock<interprocess_sharable_mutex> mapLock(*_remoteBufferMapLock);
    auto iterator = _remoteBufferMap->find(key);
    if (iterator == _remoteBufferMap->end()) {
#ifdef LOGGING_ENABLED
        BOOST_LOG_SEV(_logger, severity_levels::error) << "MAP ENTRY FOR " << key << " DOESN'T EXIST";
#endif
        _remoteReceiveBuffers.erase(key);
        return;
    }
    uint16_t len = std::get<1>(iterator->second);
    if (len != bytesReceived) {
#ifdef LOGGING_ENABLED
        BOOST_LOG_SEV(_logger, severity_levels::error) << "RECEIVED " << bytesReceived << " BYTES WHEN " << len << " WERE EXPECTED";
#endif
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
#ifdef LOGGING_ENABLED
    BOOST_LOG_SEV(_logger, startup) << "SENDER START";
#endif
    while (_isRunning.load()) {
#ifdef LOGGING_ENABLED
        BOOST_LOG_SEV(_logger, periodic) << "SENDER";
#endif
        sendRequests();
        sendACKs();
        sendData();
        boost::this_thread::sleep_for(boost::chrono::milliseconds(SENDER_DELAY));
    }
#ifdef LOGGING_ENABLED
    BOOST_LOG_SEV(_logger, teardown) << "SENDER END";
#endif
}

void dsm::Server::receiverThreadFunction() {
#ifdef LOGGING_ENABLED
    BOOST_LOG_SEV(_logger, startup) << "RECEIVER START";
#endif
    boost::system::error_code err;
    ip::udp::endpoint remoteEndpoint;
    while (_isRunning.load()) {
        _receiverSocket.receive_from(asio::buffer(_receiveBuffer), remoteEndpoint, 0, err);

#ifdef LOGGING_ENABLED
        BOOST_LOG_SEV(_logger, periodic) << "RECEIVER GOT PACKET";
#endif
        if (_receiveBuffer[0] == -1) {
#ifdef LOGGING_ENABLED
            BOOST_LOG_SEV(_logger, info) << "IGNORING PACKET";
#endif
            continue;
        }

        if (_receiveBuffer[0] >= 0) {
            processRequest(remoteEndpoint);
        }

        if (_receiveBuffer[0] < 0) {
            processACK(remoteEndpoint);
        }
    }
#ifdef LOGGING_ENABLED
    BOOST_LOG_SEV(_logger, teardown) << "RECEIVER END";
#endif
}

void dsm::Server::handlerThreadFunction() {
#ifdef LOGGING_ENABLED
    BOOST_LOG_SEV(_logger, startup) << "HANDLE RECEIVE START";
#endif
    _ioService.run();
#ifdef LOGGING_ENABLED
    BOOST_LOG_SEV(_logger, teardown) << "HANDLE RECEIVE END";
#endif
}
