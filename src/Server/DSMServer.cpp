#include "DSMServer.h"

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

dsm::Server::Server(uint8_t portOffset) :
    Base("server"+std::to_string(portOffset)),
    _isRunning(false),
    _portOffset(portOffset),
    _multicastAddress(ip::address::from_string("239.255.0."+std::to_string(portOffset+1))),
    _multicastBasePort(MULTICAST_BASE_PORT+(portOffset*MAX_CLIENTS*MAX_BUFFERS_PER_CLIENT)),
    _work(_ioService),
    _senderSocket(_ioService, ip::udp::v4()),
    _receiverSocket(_ioService, ip::udp::endpoint(ip::udp::v4(), REQUEST_BASE_PORT+_portOffset))
{
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

    for (int i = 0; i < MAX_CLIENTS; i++) {
        _multicastPortOffsets[i] = 0;
    }

    BOOST_LOG_SEV(_logger, startup) << "CONSTRUCTED SERVER";
}

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
    _segment.destroy<interprocess_upgradable_mutex>("LocalBufferMapLock");
    _segment.destroy<interprocess_upgradable_mutex>("RemoteBufferMapLock");

    /* clean up network */
    _isRunning = false;
    _ioService.stop();
    _senderThread->join();
    _receiverThread->join();
    _handlerThread->join();
    delete _senderThread;
    delete _receiverThread;
    delete _handlerThread;

    for (auto &i : _sockets) {
        delete i;
    }
    /* for (auto &i : _senderEndpoints) { */
    /*     delete i; */
    /* } */

    interprocess::message_queue::remove((_name+"_queue").c_str());
    interprocess::shared_memory_object::remove(_name.c_str());

    BOOST_LOG_SEV(_logger, teardown) << "DESTROYED SERVER";
}

void dsm::Server::start() {
    //do some work to initialize network services, etc
    //create send and receive worker threads

    _isRunning = true;
    _senderThread = new boost::thread(boost::bind(&Server::senderThreadFunction, this));
    _receiverThread = new boost::thread(boost::bind(&Server::receiverThreadFunction, this));
    _handlerThread = new boost::thread(boost::bind(&Server::handlerThreadFunction, this));

    BOOST_LOG_SEV(_logger, startup) << "MAIN LOOP START";

    while (_isRunning.load()) {
        unsigned int priority;
        interprocess::message_queue::size_type receivedSize;
        _messageQueue.receive(&_message, MESSAGE_SIZE, receivedSize, priority);

        if (receivedSize != 32) {
            break;
        }

        switch((_message.header & 0xF0) >> 4) {
            case CREATE_LOCAL:
                BOOST_LOG_SEV(_logger, info) << "LOCAL: " << _message.name << " " << _message.footer.size << " " << (_message.header & 0x0F);
                createLocalBuffer(_message.name, _message.footer.size, _message.header, false);
                break;
            case CREATE_LOCALONLY:
                BOOST_LOG_SEV(_logger, info) << "LOCALONLY: " << _message.name << " " << _message.footer.size << " " << (_message.header & 0x0F);
                createLocalBuffer(_message.name, _message.footer.size, _message.header, true);
                break;
            case CREATE_REMOTE:
                BOOST_LOG_SEV(_logger, info) << "REMOTE: " << _message.name << " " << inet_ntoa(_message.footer.ipaddr) << " " << ((_message.header >> 8) & 0x0F);
                fetchRemoteBuffer(_message.name, _message.footer.ipaddr, _message.header);
                break;
            case DISCONNECT_LOCAL:
                BOOST_LOG_SEV(_logger, info) << "REMOVE LOCAL LISTENER: " << _message.name << " " << (_message.header & 0x0F);
                disconnectLocal(_message.name, _message.header);
                break;
            case DISCONNECT_REMOTE:
                BOOST_LOG_SEV(_logger, info) << "REMOVE REMOTE LISTENER" << _message.name << " " << (_message.header & 0x0F);
                disconnectRemote(_message.name, _message.footer.ipaddr, _message.header);
                break;
            case DISCONNECT_CLIENT:
                BOOST_LOG_SEV(_logger, info) << "CLIENT DISCONNECTED: " << (_message.header & 0x0F);
                disconnectClient(_message.header);
                break;
            default:
                BOOST_LOG_SEV(_logger, severity_levels::error) << "UNKNOWN COMMAND";
                break;
        }
        _message.reset();
    }
    BOOST_LOG_SEV(_logger, teardown) << "MAIN LOOP END";
}

void dsm::Server::stop() {
    BOOST_LOG_SEV(_logger, teardown) << "SERVER STOPPING";
    _isRunning = false;
    uint8_t ignorePacket = -1;
    _senderSocket.send_to(asio::buffer(&ignorePacket, 1), ip::udp::endpoint(ip::address::from_string("127.0.0.1"), REQUEST_BASE_PORT+_portOffset));
    _messageQueue.send(0, 0, 0);
}

void dsm::Server::createLocalBuffer(LocalBufferKey key, uint16_t size, uint16_t header, bool localOnly) {
    BOOST_LOG_SEV(_logger, trace) << "CREATING LOCAL BUFFER: " << key;
    uint8_t clientID = header & 0x0F;
    interprocess::scoped_lock<interprocess_upgradable_mutex> mapLock(*_localBufferMapLock);
    if (_localBufferMap->find(key) != _localBufferMap->end()) {
        _localBufferLocalListeners[key].insert(clientID);
        _clientSubscriptions[clientID].first.insert(key);
        return;
    } else {
        if (_multicastPortOffsets[clientID] > MAX_BUFFERS_PER_CLIENT-1) {
            BOOST_LOG_SEV(_logger, severity_levels::error) << "CLIENT " << clientID << " HAS TOO MANY LOCAL BUFFERS";
            return;
        }
    }

    _localBufferLocalListeners[key].insert(clientID);
    _clientSubscriptions[clientID].first.insert(key);

    //TODO make these 2 allocate calls into one
    void* buf = _segment.allocate(size);
    interprocess::managed_shared_memory::handle_t handle = _segment.get_handle_from_address(buf);
    interprocess::offset_ptr<interprocess_upgradable_mutex> mutex = static_cast<interprocess_upgradable_mutex*>(_segment.allocate(sizeof(interprocess_upgradable_mutex)));
    new (mutex.get()) interprocess_upgradable_mutex;
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
    BOOST_LOG_SEV(_logger, trace) << "CREATING REMOTE BUFFER: " << key;

    void* buf = _segment.allocate(size);
    interprocess::managed_shared_memory::handle_t handle = _segment.get_handle_from_address(buf);
    interprocess::offset_ptr<interprocess_upgradable_mutex> mutex = static_cast<interprocess_upgradable_mutex*>(_segment.allocate(sizeof(interprocess_upgradable_mutex)));
    new (mutex.get()) interprocess_upgradable_mutex;

    interprocess::scoped_lock<interprocess_upgradable_mutex> mapLock(*_remoteBufferMapLock);
    _remoteBufferMap->insert(std::make_pair(key, std::make_tuple(handle, size, mutex)));
}

void dsm::Server::fetchRemoteBuffer(std::string name, struct in_addr addr, uint16_t header) {
    uint8_t clientID = header & 0x0F;
    std::string ipaddr = inet_ntoa(addr);
    uint8_t portOffset = (header >> 8) & 0x0F;
    ip::udp::endpoint endpoint(ip::address::from_string(ipaddr), REQUEST_BASE_PORT+portOffset);
    RemoteBufferKey key(name, endpoint);
    BOOST_LOG_SEV(_logger, trace) << "FETCHING REMOTE BUFFER: " << key;
    _remoteBufferLocalListeners[key].insert(clientID);
    _clientSubscriptions[clientID].second.insert(key);

    interprocess::sharable_lock<interprocess_upgradable_mutex> mapLock(*_remoteBufferMapLock);
    //TODO There must be a better way
    if (_remoteBufferMap->find(key) != _remoteBufferMap->end()) {
        return;
    }
    mapLock.unlock();

    boost::unique_lock<boost::shared_mutex> fetchLock(_remoteBuffersToFetchMutex);
    _remoteBuffersToFetch.insert(key);
}

void dsm::Server::disconnectLocal(std::string name, uint16_t header) {
    uint8_t clientID = header & 0x0F;
    BOOST_LOG_SEV(_logger, trace) << "CLIENT " << clientID << "DISCONNECTED FROM LOCAL " << name;
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
    BOOST_LOG_SEV(_logger, trace) << "CLIENT " << clientID << "DISCONNECTED FROM REMOTE " << key;
    _remoteBufferLocalListeners[key].erase(clientID);
    _clientSubscriptions[clientID].second.erase(key);
    if (_remoteBufferLocalListeners[key].empty()) {
        removeRemoteBuffer(key);
    }
}

void dsm::Server::disconnectClient(uint16_t header) {
    uint8_t clientID = (header & 0x0F);
    BOOST_LOG_SEV(_logger, trace) << "CLIENT " << clientID << " DISCONNECTED FROM SERVER";
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
    BOOST_LOG_SEV(_logger, trace) << "REMOVING LOCAL BUFFER " << key;
    interprocess::scoped_lock<interprocess_upgradable_mutex> mapLock(*_localBufferMapLock);
    auto iterator = _localBufferMap->find(key);
    if (iterator == _localBufferMap->end()) {
        return;
    }
    _segment.deallocate(_segment.get_address_from_handle(std::get<0>(iterator->second)));
    _segment.deallocate(std::get<2>(iterator->second).get());
    _localBufferMap->erase(key);
}

void dsm::Server::removeRemoteBuffer(RemoteBufferKey key) {
    BOOST_LOG_SEV(_logger, trace) << "REMOVING REMOTE BUFFER " << key;
    interprocess::scoped_lock<interprocess_upgradable_mutex> mapLock(*_remoteBufferMapLock);
    auto iterator = _remoteBufferMap->find(key);
    if (iterator == _remoteBufferMap->end()) {
        return;
    }
    _segment.deallocate(_segment.get_address_from_handle(std::get<0>(iterator->second)));
    _segment.deallocate(std::get<2>(iterator->second).get());
    _remoteBufferMap->erase(key);
}

void dsm::Server::sendRequests() {
    //TODO could lock mutex, get values, then unlock
    boost::shared_lock<boost::shared_mutex> lock(_remoteBuffersToFetchMutex);
    for (auto const &i : _remoteBuffersToFetch) {
        BOOST_LOG_SEV(_logger, trace) << "SENDING REQUEST " << i;
        boost::array<char, 28> sendBuffer;
        sendBuffer[0] = _portOffset;
        sendBuffer[1] = i.name.length();
        std::strcpy(&sendBuffer[2], i.name.c_str());
        /* _senderSocket.send_to(buffer(sendBuffer), i.second); */
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
    interprocess::sharable_lock<interprocess_upgradable_mutex> mapLock(*_localBufferMapLock);
    for (auto &i : _remoteServersToACK) {
        //multicast lock
        auto iterator = _localBufferMap->find(i.first);
        if  (iterator == _localBufferMap->end()) {
            continue;
        }
        uint16_t multicastPort = std::get<3>(iterator->second).port();
        if (multicastPort == 0) {
            //TODO? specific code to tell remote that buffer is local only
            BOOST_LOG_SEV(_logger, severity_levels::error) << "BUFFER " << i.first << " IS LOCAL ONLY";
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
            BOOST_LOG_SEV(_logger, trace) << "SENDING ACK " << i.first << " " << j.address().to_v4().to_string() << " " << j.port();
            /* _senderSocket.send_to(buffer(sendBuffer), j); */
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
    interprocess::sharable_lock<interprocess_upgradable_mutex> mapLock(*_localBufferMapLock);
    for (auto const &i : *_localBufferMap) {
        BOOST_LOG_SEV(_logger, periodic) << "SENDING DATA {" << i.first << ", " << std::get<3>(i.second).address() << ", " << std::get<3>(i.second).port() << "}";
        ip::udp::endpoint endpoint = std::get<3>(i.second);
        if (endpoint.port() == 0) {
            continue;
        }
        void* data = _segment.get_address_from_handle(std::get<0>(i.second));
        uint16_t len = std::get<1>(i.second);
        interprocess::sharable_lock<interprocess_upgradable_mutex> dataLock(*(std::get<2>(i.second).get()));
        /* _senderSocket.send_to(buffer(data, len), i.second); */
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
    uint8_t len = (uint8_t)_receiveBuffer[1];
    std::string name(&_receiveBuffer[2], len);
    remoteEndpoint.port(REQUEST_BASE_PORT+_receiveBuffer[0]);
    BOOST_LOG_SEV(_logger, info) << "RECEIVED REQUEST FOR " << name << " FROM " << remoteEndpoint.address().to_string() << " " << remoteEndpoint.port();
    interprocess::sharable_lock<interprocess_upgradable_mutex> mapLock(*_localBufferMapLock);
    if (_localBufferMap->find(name) != _localBufferMap->end()) {
        mapLock.unlock();
        boost::unique_lock<boost::shared_mutex> lock(_remoteServersToACKMutex);
        _remoteServersToACK[name].push_back(remoteEndpoint);
    } else {
        BOOST_LOG_SEV(_logger, severity_levels::error) << "COULDN'T FIND " << name;
    }
}

void dsm::Server::processACK(ip::udp::endpoint remoteEndpoint) {
    std::string name(&_receiveBuffer[10], _receiveBuffer[1]);
    remoteEndpoint.port(REQUEST_BASE_PORT+(_receiveBuffer[0] & 0x0F));
    RemoteBufferKey key(name, remoteEndpoint);
    {
        //check if <name, addr, port> exists in remotes to create
        BOOST_LOG_SEV(_logger, info) << "RECEIVED ACK FOR " << key;
        if (_remoteBuffersToFetch.find(key) == _remoteBuffersToFetch.end()) {
            return;
        }
        //delete entry if true and continue, else return
        boost::unique_lock<boost::shared_mutex> lock(_remoteBuffersToFetchMutex);
        _remoteBuffersToFetch.erase(key);
    }
    {
        //get the buffer length and create it
        uint16_t buflen;
        memcpy(&buflen, &_receiveBuffer[2], sizeof(uint16_t));
        createRemoteBuffer(key, buflen);
    }
    {
        //create socket and start handler
        uint32_t mcastaddr;
        memcpy(&mcastaddr, &_receiveBuffer[4], sizeof(mcastaddr));
        uint16_t mcastport;
        memcpy(&mcastport, &_receiveBuffer[8], sizeof(mcastport));

        ip::udp::socket* sock = new ip::udp::socket(_ioService);
        ip::udp::endpoint listenEndpoint(ip::address_v4::from_string("0.0.0.0"), mcastport);
        sock->open(ip::udp::v4());
        sock->set_option(ip::udp::socket::reuse_address(true));
        sock->bind(listenEndpoint);
        sock->set_option(ip::multicast::join_group(ip::address_v4(mcastaddr)));
        _remoteReceiveBuffers[key] = boost::array<char, 256>();
        /* ip::udp::endpoint *senderEndpoint = new ip::udp::endpoint(); */
        ip::udp::endpoint senderEndpoint;
        sock->async_receive_from(asio::buffer(_remoteReceiveBuffers[key]),
                                 senderEndpoint,
                                 boost::bind(&dsm::Server::processData,
                                             this,
                                             asio::placeholders::error,
                                             asio::placeholders::bytes_transferred,
                                             key,
                                             sock,
                                             senderEndpoint));
        /* _senderEndpoints.push_back(senderEndpoint); */
        _sockets.push_back(sock);
    }
}

void dsm::Server::processData(const boost::system::error_code &error, size_t bytesReceived, RemoteBufferKey key, ip::udp::socket* sock, ip::udp::endpoint sender) {
    BOOST_LOG_SEV(_logger, periodic) << "RECEIVED DATA FOR REMOTE " << key;
    if (error) {
        BOOST_LOG_SEV(_logger, severity_levels::error) << "ERROR PROCESSING DATA";
        return;
    }
    interprocess::sharable_lock<interprocess_upgradable_mutex> mapLock(*_remoteBufferMapLock);
    RemoteBuffer buf;
    try {
        buf = _remoteBufferMap->at(key);
    } catch (std::exception const& e) {
        BOOST_LOG_SEV(_logger, severity_levels::error) << "MAP ENTRY FOR " << key << " DOESN'T EXIST";
        return;
    }
    uint16_t len = std::get<1>(buf);
    if (len != bytesReceived) {
        BOOST_LOG_SEV(_logger, severity_levels::error) << "RECEIVED " << bytesReceived << " BYTES WHEN " << len << " WERE EXPECTED";
        return;
    }
    void* ptr = _segment.get_address_from_handle(std::get<0>(buf));
    interprocess::scoped_lock<interprocess_upgradable_mutex> dataLock(*(std::get<2>(buf).get()));
    memcpy(ptr, _remoteReceiveBuffers[key].data(), len);
    dataLock.unlock();
    mapLock.unlock();
    sock->async_receive_from(asio::buffer(_remoteReceiveBuffers[key]),
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
    BOOST_LOG_SEV(_logger, startup) << "SENDER START";
    while (_isRunning.load()) {
        BOOST_LOG_SEV(_logger, periodic) << "SENDER";
        sendRequests();
        sendACKs();
        sendData();
        boost::this_thread::sleep_for(boost::chrono::milliseconds(SENDER_DELAY));
    }
    BOOST_LOG_SEV(_logger, teardown) << "SENDER END";
}

void dsm::Server::receiverThreadFunction() {
    BOOST_LOG_SEV(_logger, startup) << "RECEIVER START";
    while (_isRunning.load()) {
        boost::system::error_code err;
        ip::udp::endpoint remoteEndpoint;
        _receiverSocket.receive_from(asio::buffer(_receiveBuffer), remoteEndpoint, 0, err);

        BOOST_LOG_SEV(_logger, periodic) << "RECEIVER GOT PACKET";
        if (_receiveBuffer[0] == -1) {
            BOOST_LOG_SEV(_logger, info) << "IGNORING PACKET";
            continue;
        }

        if (_receiveBuffer[0] >= 0) {
            processRequest(remoteEndpoint);
        }

        if (_receiveBuffer[0] < 0) {
            processACK(remoteEndpoint);
        }
    }
    BOOST_LOG_SEV(_logger, teardown) << "RECEIVER END";
}

void dsm::Server::handlerThreadFunction() {
    BOOST_LOG_SEV(_logger, startup) << "HANDLE RECEIVE START";
    _ioService.run();
    BOOST_LOG_SEV(_logger, teardown) << "HANDLE RECEIVE END";
}
