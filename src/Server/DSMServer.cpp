#include "DSMServer.h"

std::ostream& operator<<(std::ostream& stream, severity_levels level)
{
    static const char* strings[] =
    {
        "trace   ",
        "startup ",
        "teardown",
        "info    ",
        "error   ",
        "debug   "
    };
    if (level >= 0 && level < 5) {
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
    _multicastBasePort(MULTICAST_BASE_PORT+(portOffset*1024)),
    _multicastPortOffset(0),
    _work(_ioService),
    _senderSocket(_ioService, ip::udp::v4()),
    _receiverSocket(_ioService, ip::udp::endpoint(ip::udp::v4(), REQUEST_BASE_PORT+_portOffset))
{
    boost::log::formatter format = boost::log::expressions::stream <<
        "[" << severity << "] " <<
        boost::log::expressions::smessage;
    /* boost::log::core::get()->set_filter(severity >= severity_levels::trace); */

    typedef boost::log::sinks::synchronous_sink<boost::log::sinks::text_ostream_backend> text_sink;
    boost::shared_ptr<text_sink> sink = boost::make_shared<text_sink>();
    boost::shared_ptr<std::ostream> stream(&std::clog, boost::null_deleter());
    sink->locked_backend()->add_stream(stream);
    sink->set_formatter(format);
    boost::log::core::get()->add_sink(sink);
}

dsm::Server::~Server() {
    BOOST_LOG_SEV(_logger, teardown) << "DESTRUCTOR START";
    for (auto const &i : *_localBufferMap) {
        _segment.deallocate(_segment.get_address_from_handle(std::get<0>(i.second)));
        _segment.deallocate(std::get<2>(i.second).get());
    }
    for (auto const &i : *_remoteBufferMap) {
        _segment.deallocate(_segment.get_address_from_handle(std::get<0>(i.second)));
        _segment.deallocate(std::get<2>(i.second).get());
    }
    _segment.destroy<BufferMap>("LocalBufferMap");
    _segment.destroy<BufferMap>("RemoteBufferMap");
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
    for (auto &i : _senderEndpoints) {
        delete i;
    }

    message_queue::remove((_name+"_queue").c_str());
    shared_memory_object::remove(_name.c_str());
    BOOST_LOG_SEV(_logger, teardown) << "DESTRUCTOR END";
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
        message_queue::size_type receivedSize;
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
                BOOST_LOG_SEV(_logger, severity_levels::error) << "UNKNOWN";
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
    _senderSocket.send_to(buffer(&ignorePacket, 1), ip::udp::endpoint(ip::address::from_string("127.0.0.1"), REQUEST_BASE_PORT+_portOffset));
    _messageQueue.send(0, 0, 0);
}

void dsm::Server::createLocalBuffer(std::string name, uint16_t size, uint16_t header, bool localOnly) {
    uint8_t clientID = header & 0x0F;
    _localBufferLocalListeners[name].insert(clientID);
    _clientSubscriptions[clientID].first.insert(name);
    boost::unique_lock<boost::shared_mutex> buffersLock(_createdLocalBuffersMutex);
    if (_createdLocalBuffers.find(name) != _createdLocalBuffers.end()) {
        return;
    }
    _createdLocalBuffers.insert(name);
    buffersLock.unlock();

    scoped_lock<interprocess_upgradable_mutex> mapLock(*_localBufferMapLock, defer_lock);
    //TODO make these 2 allocate calls into one
    void* buf = _segment.allocate(size);
    managed_shared_memory::handle_t handle = _segment.get_handle_from_address(buf);
    offset_ptr<interprocess_upgradable_mutex> mutex = static_cast<interprocess_upgradable_mutex*>(_segment.allocate(sizeof(interprocess_upgradable_mutex)));
    new (mutex.get()) interprocess_upgradable_mutex;

    if (!localOnly) {
        boost::unique_lock<boost::shared_mutex> multicastLock(_localBufferMulticastAddressesMutex);
        _localBufferMulticastAddresses.insert(std::make_pair(name, ip::udp::endpoint(_multicastAddress, _multicastBasePort+_multicastPortOffset)));
        _multicastPortOffset++;
        multicastLock.unlock();
    }

    mapLock.lock();
    _localBufferMap->insert(std::make_pair(name, std::make_tuple(handle, size, mutex)));
    mapLock.unlock();
}

void dsm::Server::createRemoteBuffer(std::string name, std::string ipaddr, uint16_t size) {
    scoped_lock<interprocess_upgradable_mutex> lock(*_remoteBufferMapLock, defer_lock);

    void* buf = _segment.allocate(size);
    managed_shared_memory::handle_t handle = _segment.get_handle_from_address(buf);
    offset_ptr<interprocess_upgradable_mutex> mutex = static_cast<interprocess_upgradable_mutex*>(_segment.allocate(sizeof(interprocess_upgradable_mutex)));
    new (mutex.get()) interprocess_upgradable_mutex;

    boost::unique_lock<boost::shared_mutex> buffersLock(_createdRemoteBuffersMutex);
    _createdRemoteBuffers.insert(ipaddr+name);
    buffersLock.unlock();

    lock.lock();
    _remoteBufferMap->insert(std::make_pair(ipaddr+name, std::make_tuple(handle, size, mutex)));
    lock.unlock();
}

void dsm::Server::fetchRemoteBuffer(std::string name, struct in_addr addr, uint16_t header) {
    uint8_t clientID = header & 0x0F;
    std::string ipaddr = inet_ntoa(addr);
    uint8_t portOffset = (header >> 8) & 0x0F;
    _remoteBufferLocalListeners[ipaddr+name].insert(clientID);
    _clientSubscriptions[clientID].second.insert(std::make_pair(name, ipaddr));

    boost::shared_lock<boost::shared_mutex> buffersLock(_createdRemoteBuffersMutex);
    if (_createdRemoteBuffers.find(ipaddr+name) != _createdRemoteBuffers.end()) {
        return;
    }
    buffersLock.unlock();

    boost::unique_lock<boost::shared_mutex> lock(_remoteBuffersToFetchMutex);
    _remoteBuffersToFetch.insert(std::make_pair(name, ip::udp::endpoint(ip::address::from_string(ipaddr), REQUEST_BASE_PORT+portOffset)));
    lock.unlock();
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
    _remoteBufferLocalListeners[ipaddr+name].erase(clientID);
    _clientSubscriptions[clientID].second.erase(std::make_pair(name, ipaddr));
    if (_remoteBufferLocalListeners[ipaddr+name].empty()) {
        removeRemoteBuffer(name, ipaddr);
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
        _remoteBufferLocalListeners[i.first+i.second].erase(clientID);
        if (_remoteBufferLocalListeners[i.first+i.second].empty()) {
            removeRemoteBuffer(i.first, i.second);
        }
    }
    _clientSubscriptions.erase(clientID);
}

void dsm::Server::removeLocalBuffer(std::string name) {
    BOOST_LOG_SEV(_logger, info) << "REMOVING LOCAL BUFFER " << name;
    boost::unique_lock<boost::shared_mutex> buffersLock(_createdLocalBuffersMutex);
    if (_createdLocalBuffers.find(name) == _createdLocalBuffers.end()) {
        return;
    }
    _createdLocalBuffers.erase(name);
    buffersLock.unlock();

    boost::unique_lock<boost::shared_mutex> multicastLock(_localBufferMulticastAddressesMutex);
    _localBufferMulticastAddresses.erase(name);
    multicastLock.unlock();

    scoped_lock<interprocess_upgradable_mutex> mapLock(*_localBufferMapLock);
    Buffer buf = (*_localBufferMap)[name];
    _segment.deallocate(_segment.get_address_from_handle(std::get<0>(buf)));
    _segment.deallocate(std::get<2>(buf).get());
    _localBufferMap->erase(name);
    mapLock.unlock();
}

void dsm::Server::removeRemoteBuffer(std::string name, std::string ipaddr) {
    BOOST_LOG_SEV(_logger, info) << "REMOVING REMOTE BUFFER " << name << " FROM " << ipaddr;
    boost::unique_lock<boost::shared_mutex> buffersLock(_createdRemoteBuffersMutex);
    if (_createdRemoteBuffers.find(ipaddr+name) == _createdRemoteBuffers.end()) {
        return;
    }
    _createdRemoteBuffers.erase(ipaddr+name);
    buffersLock.unlock();

    scoped_lock<interprocess_upgradable_mutex> lock(*_localBufferMapLock);
    Buffer buf = (*_remoteBufferMap)[ipaddr+name];
    _segment.deallocate(_segment.get_address_from_handle(std::get<0>(buf)));
    _segment.deallocate(std::get<2>(buf).get());
    _remoteBufferMap->erase(ipaddr+name);
    lock.unlock();
}

void dsm::Server::sendRequests() {
    //TODO could lock mutex, get values, then unlock
    boost::shared_lock<boost::shared_mutex> lock(_remoteBuffersToFetchMutex);
    for (auto const &i : _remoteBuffersToFetch) {
        BOOST_LOG_SEV(_logger, trace) << "SENDING REQUEST " << i.first;
        boost::array<char, 28> sendBuffer;
        sendBuffer[0] = _portOffset;
        sendBuffer[1] = i.first.length();
        std::strcpy(&sendBuffer[2], i.first.c_str());
        /* _senderSocket.send_to(buffer(sendBuffer), i.second); */
        _senderSocket.async_send_to(buffer(sendBuffer),
                                    i.second,
                                    boost::bind(&dsm::Server::sendHandler,
                                                this,
                                                boost::asio::placeholders::error,
                                                boost::asio::placeholders::bytes_transferred));
    }
}

void dsm::Server::sendACKs() {
    boost::upgrade_lock<boost::shared_mutex> ackLock(_remoteServersToACKMutex);
    boost::shared_lock<boost::shared_mutex> multicastLock(_localBufferMulticastAddressesMutex);
    sharable_lock<interprocess_upgradable_mutex> mapLock(*_localBufferMapLock);
    for (auto &i : _remoteServersToACK) {
        BOOST_LOG_SEV(_logger, trace) << "ACKING: " << i.first;
        //multicast lock
        uint32_t multicastAddress;
        uint16_t multicastPort;
        auto iterator = _localBufferMulticastAddresses.find(i.first);
        if (iterator != _localBufferMulticastAddresses.end()) {
            multicastAddress = iterator->second.address().to_v4().to_ulong();
            multicastPort = iterator->second.port();
        } else {
            //TODO? specific code to tell remote that buffer is local only
            BOOST_LOG_SEV(_logger, severity_levels::error) << "BUFFER " << i.first << " IS LOCAL ONLY";
            continue;
        }
        //multicast unlock

        //mapLock lock
        uint16_t len = std::get<1>((*_localBufferMap)[i.first]);
        //mapLock unlock

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
            BOOST_LOG_SEV(_logger, trace) << "SENDING ACK: " << j.address() << " " << j.port();
            /* _senderSocket.send_to(buffer(sendBuffer), j); */
            _senderSocket.async_send_to(buffer(sendBuffer),
                                        j,
                                        boost::bind(&dsm::Server::sendHandler,
                                                    this,
                                                    boost::asio::placeholders::error,
                                                    boost::asio::placeholders::bytes_transferred));
        }
    }
    mapLock.unlock();
    multicastLock.unlock();

    boost::upgrade_to_unique_lock<boost::shared_mutex> uniqueLock(ackLock);
    _remoteServersToACK.clear();
}

void dsm::Server::sendData() {
    boost::shared_lock<boost::shared_mutex> multicastLock(_localBufferMulticastAddressesMutex);
    sharable_lock<interprocess_upgradable_mutex> mapLock(*_localBufferMapLock);
    for (auto const &i : _localBufferMulticastAddresses) {
        BOOST_LOG_SEV(_logger, trace) << "SENDING DATA FOR " << i.first << " TO " << i.second.address() << " " << i.second.port();
        Buffer buf = (*_localBufferMap)[i.first];
        void* data = _segment.get_address_from_handle(std::get<0>(buf));
        uint16_t len = std::get<1>(buf);
        sharable_lock<interprocess_upgradable_mutex> dataLock(*(std::get<2>(buf).get()));
        /* _senderSocket.send_to(buffer(data, len), i.second); */
        _senderSocket.async_send_to(buffer(data,len),
                                    i.second,
                                    boost::bind(&dsm::Server::sendHandler,
                                                this,
                                                boost::asio::placeholders::error,
                                                boost::asio::placeholders::bytes_transferred));
    }
}

void dsm::Server::sendHandler(const boost::system::error_code&, std::size_t) {}

void dsm::Server::processRequest(ip::udp::endpoint remoteEndpoint) {
    uint8_t len = (uint8_t)_receiveBuffer[1];
    std::string name(&_receiveBuffer[2], len);
    BOOST_LOG_SEV(_logger, info) << "RECEIVED REQUEST FOR " << name;
    boost::shared_lock<boost::shared_mutex> buffersLock(_createdLocalBuffersMutex);
    if (_createdLocalBuffers.find(name) != _createdLocalBuffers.end()) {
        buffersLock.unlock();
        remoteEndpoint.port(REQUEST_BASE_PORT+_receiveBuffer[0]);
        boost::unique_lock<boost::shared_mutex> lock(_remoteServersToACKMutex);
        _remoteServersToACK[name].insert(remoteEndpoint);
    } else {
        BOOST_LOG_SEV(_logger, severity_levels::error) << "BUFFER NOT FOUND";
    }
}

void dsm::Server::processACK(ip::udp::endpoint remoteEndpoint) {
    std::string name(&_receiveBuffer[10], _receiveBuffer[1]);
    {
        //check if <name, addr, port> exists in remotes to create
        remoteEndpoint.port(REQUEST_BASE_PORT+(_receiveBuffer[0] & 0x0F));
        BOOST_LOG_SEV(_logger, info) << "RECEIVED ACK FOR " << name << " " << remoteEndpoint.address().to_string() << " " << remoteEndpoint.port();
        if (_remoteBuffersToFetch.find(std::make_pair(name, remoteEndpoint)) == _remoteBuffersToFetch.end()) {
            return;
        }
        //delete entry if true and continue, else return
        boost::unique_lock<boost::shared_mutex> lock(_remoteBuffersToFetchMutex);
        _remoteBuffersToFetch.erase(std::make_pair(name, remoteEndpoint));
    }
    {
        //get the buffer length and create it
        uint16_t buflen;
        memcpy(&buflen, &_receiveBuffer[2], sizeof(uint16_t));
        createRemoteBuffer(name, remoteEndpoint.address().to_string(), buflen);
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
        //mcastv4 is unnecessary but we want to log
        ip::address_v4 mcastv4(mcastaddr);
        BOOST_LOG_SEV(_logger, info) << "ACK MULTICAST " << mcastv4.to_string() << " " << mcastport;
        //this doesn't use mcastv4 purposely
        sock->set_option(ip::multicast::join_group(ip::address_v4(mcastaddr)));
        std::string receiveBufferKey = remoteEndpoint.address().to_string()+name;
        _remoteReceiveBuffers[receiveBufferKey] = boost::array<char, 256>();
        ip::udp::endpoint *senderEndpoint = new ip::udp::endpoint();
        sock->async_receive_from(buffer(_remoteReceiveBuffers[receiveBufferKey]),
                                 *senderEndpoint,
                                 boost::bind(&dsm::Server::processData,
                                             this,
                                             placeholders::error,
                                             placeholders::bytes_transferred,
                                             name,
                                             remoteEndpoint,
                                             sock,
                                             senderEndpoint));
        _senderEndpoints.push_back(senderEndpoint);
        _sockets.push_back(sock);
    }
}

void dsm::Server::processData(const boost::system::error_code &error, size_t bytesReceived, std::string name, ip::udp::endpoint remoteEndpoint, ip::udp::socket* sock, ip::udp::endpoint* sender) {
    BOOST_LOG_SEV(_logger, trace) << "PROCESS DATA START";
    if (error) {
        BOOST_LOG_SEV(_logger, severity_levels::error) << "ERROR IN PROCESS DATA";
        return;
    }
    std::string ipaddr = remoteEndpoint.address().to_string();
    BOOST_LOG_SEV(_logger, info) << _remoteReceiveBuffers[ipaddr+name].data();
    sharable_lock<interprocess_upgradable_mutex> mapLock(*_remoteBufferMapLock);
    Buffer buf = (*_remoteBufferMap)[ipaddr+name];
    uint16_t len = std::get<1>(buf);
    if (len != bytesReceived) {
        BOOST_LOG_SEV(_logger, severity_levels::error) << "LENGTHS NOT EQUAL";
        return;
    }
    void* ptr = _segment.get_address_from_handle(std::get<0>(buf));
    scoped_lock<interprocess_upgradable_mutex> dataLock(*(std::get<2>(buf).get()));
    memcpy(ptr, _remoteReceiveBuffers[ipaddr+name].data(), len);
    mapLock.unlock();
    dataLock.unlock();
    sock->async_receive_from(buffer(_remoteReceiveBuffers[ipaddr+name]),
                             *sender,
                             boost::bind(&dsm::Server::processData,
                                         this,
                                         placeholders::error,
                                         placeholders::bytes_transferred,
                                         name,
                                         remoteEndpoint,
                                         sock,
                                         sender));
}

void dsm::Server::senderThreadFunction() {
    BOOST_LOG_SEV(_logger, startup) << "SENDER START";
    while (_isRunning.load()) {
        BOOST_LOG_SEV(_logger, trace) << "SENDER";
        sendRequests();
        sendACKs();
        sendData();
        boost::this_thread::sleep_for(boost::chrono::seconds(SENDER_DELAY));
    }
}

void dsm::Server::receiverThreadFunction() {
    BOOST_LOG_SEV(_logger, startup) << "RECEIVER START";
    while (_isRunning.load()) {
        boost::system::error_code err;
        ip::udp::endpoint remoteEndpoint;
        _receiverSocket.receive_from(buffer(_receiveBuffer), remoteEndpoint, 0, err);

        BOOST_LOG_SEV(_logger, trace) << "RECEIVER GOT PACKET";
        if (_receiveBuffer[0] == -1) {
            BOOST_LOG_SEV(_logger, teardown) << "IGNORING PACKET";
            continue;
        }

        if (_receiveBuffer[0] >= 0) {
            processRequest(remoteEndpoint);
        }

        if (_receiveBuffer[0] < 0) {
            processACK(remoteEndpoint);
        }
    }
}

void dsm::Server::handlerThreadFunction() {
    BOOST_LOG_SEV(_logger, startup) << "HANDLE RECEIVE START";
    _ioService.run();
    BOOST_LOG_SEV(_logger, teardown) << "HANDLE RECEIVE END";
}
