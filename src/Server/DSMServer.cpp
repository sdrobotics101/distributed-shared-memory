#include "DSMServer.h"

dsm::Server::Server(std::string name, uint8_t portOffset, std::string multicastAddress, uint16_t multicastBasePort) :
    Base(name),
    _isRunning(false),
    _portOffset(portOffset),
    _multicastAddress(ip::address::from_string(multicastAddress)),
    _multicastBasePort(multicastBasePort),
    _multicastPortOffset(0)
{
    _ioService = new io_service();
    _senderSocket = new ip::udp::socket(*_ioService);
    _senderSocket->open(ip::udp::v4());
    _receiverSocket = new ip::udp::socket(*_ioService, ip::udp::endpoint(ip::udp::v4(), BASE_PORT+_portOffset));
}

dsm::Server::~Server() {
    std::cout << "DESTRUCTOR START" << std::endl;
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
    _ioService->stop();
    _senderThread->join();
    _receiverThread->join();
    delete _senderThread;
    delete _receiverThread;

    delete _senderSocket;
    delete _receiverSocket;
    for (auto &i : _sockets) {
        delete i;
    }

    delete _ioService;

    message_queue::remove((_name+"_queue").c_str());
    shared_memory_object::remove(_name.c_str());
    std::cout << "DESTRUCTOR END" << std::endl;
}

void dsm::Server::start() {
    //do some work to initialize network services, etc
    //create send and receive worker threads

    _isRunning = true;
    _senderThread = new boost::thread(boost::bind(&Server::senderThreadFunction, this));
    _receiverThread = new boost::thread(boost::bind(&Server::receiverThreadFunction, this));

    while (1) {
        unsigned int priority;
        message_queue::size_type receivedSize;
        _messageQueue.receive(&_message, MESSAGE_SIZE, receivedSize, priority);

        //TODO bring this to while loop condition and use receivedSize as continue condition
        if (!_isRunning.load()) {
            std::cout << "BREAKING" << std::endl;
            break;
        }

        switch((_message.header & 0xF0) >> 4) {
            case CREATE_LOCAL:
                std::cout << "LOCAL: " << _message.name << " " << _message.footer.size << " " << (_message.header & 0x0F) << std::endl;
                createLocalBuffer(_message.name, _message.footer.size, _message.header);
                break;
            case CREATE_REMOTE:
                std::cout << "REMOTE: " << _message.name << " " << inet_ntoa(_message.footer.ipaddr) << " " << ((_message.header >> 8) & 0x0F) << std::endl;
                fetchRemoteBuffer(_message.name, _message.footer.ipaddr, _message.header);
                break;
            case DISCONNECT_LOCAL:
                std::cout << "REMOVE LOCAL LISTENER: " << _message.name << " " << (_message.header & 0x0F) << std::endl;
                disconnectLocal(_message.name, _message.header);
                break;
            default:
                std::cout << "UNKNOWN" << std::endl;
        }
        _message.reset();
    }
}

void dsm::Server::stop() {
    std::cout << "STOPPING" << std::endl;
    _isRunning = false;
    uint8_t ignorePacket = -1;
    _senderSocket->send_to(buffer(&ignorePacket, 1), ip::udp::endpoint(ip::address::from_string("127.0.0.1"), BASE_PORT+_portOffset));
    _messageQueue.send(0, 0, 0);
}

void dsm::Server::createLocalBuffer(std::string name, uint16_t size, uint16_t header) {
    uint8_t clientID = header & 0x0F;
    _localBufferLocalListeners[_message.name].insert(clientID);
    if (_createdLocalBuffers.find(name) != _createdLocalBuffers.end()) {
        return;
    }
    //TODO make these 2 allocate calls into one
    void* buf = _segment.allocate(size);
    managed_shared_memory::handle_t handle = _segment.get_handle_from_address(buf);
    offset_ptr<interprocess_upgradable_mutex> mutex = static_cast<interprocess_upgradable_mutex*>(_segment.allocate(sizeof(interprocess_upgradable_mutex)));
    new (mutex.get()) interprocess_upgradable_mutex;

    _createdLocalBuffers.insert(name);

    {
        boost::unique_lock<boost::shared_mutex> lock(_localBufferMulticastAddressesMutex);
        _localBufferMulticastAddresses.insert(std::make_pair(name, ip::udp::endpoint(_multicastAddress, _multicastBasePort+_multicastPortOffset)));
        _multicastPortOffset++;
    }
    scoped_lock<interprocess_upgradable_mutex> lock(*_localBufferMapLock);
    _localBufferMap->insert(std::make_pair(name, std::make_tuple(handle, size, mutex)));
}

void dsm::Server::createRemoteBuffer(std::string name, std::string ipaddr, uint16_t size) {
    void* buf = _segment.allocate(size);
    managed_shared_memory::handle_t handle = _segment.get_handle_from_address(buf);
    offset_ptr<interprocess_upgradable_mutex> mutex = static_cast<interprocess_upgradable_mutex*>(_segment.allocate(sizeof(interprocess_upgradable_mutex)));
    new (mutex.get()) interprocess_upgradable_mutex;

    _createdRemoteBuffers.insert(ipaddr+name);

    scoped_lock<interprocess_upgradable_mutex> lock(*_remoteBufferMapLock);
    _remoteBufferMap->insert(std::make_pair(ipaddr+name, std::make_tuple(handle, size, mutex)));
}

void dsm::Server::fetchRemoteBuffer(std::string name, struct in_addr addr, uint16_t header) {
    uint8_t clientID = header & 0x0F;
    std::string ipaddr = inet_ntoa(addr);
    uint8_t portOffset = (header >> 8) & 0x0F;
    _remoteBufferLocalListeners[ipaddr+name].insert(clientID);

    if (_createdRemoteBuffers.find(ipaddr+name) != _createdRemoteBuffers.end()) {
        return;
    }

    boost::unique_lock<boost::shared_mutex> lock(_remoteBuffersToFetchMutex);
    _remoteBuffersToFetch.insert(std::make_pair(name, ip::udp::endpoint(ip::address::from_string(ipaddr), BASE_PORT+portOffset)));
}

void dsm::Server::disconnectLocal(std::string name, uint16_t header) {
    _localBufferLocalListeners[name].erase(header & 0x0F);
    if (_localBufferLocalListeners[name].empty()) {
        std::cout << "REMOVING LOCAL BUFFER " << name << std::endl;
        removeLocalBuffer(name);
    }
}

void dsm::Server::disconnectRemote(std::string name, struct in_addr addr, uint16_t header) {
    std::string ipaddr = inet_ntoa(addr);
    _remoteBufferLocalListeners[ipaddr+name].erase(header & 0x0F);
    if (_remoteBufferLocalListeners[ipaddr+name].empty()) {
        std::cout << "REMOVING REMOTE BUFFER " << ipaddr+name << std::endl;
        removeRemoteBuffer(name, ipaddr);
    }
}

void dsm::Server::removeLocalBuffer(std::string name) {
    if (_createdLocalBuffers.find(name) == _createdLocalBuffers.end()) {
        return;
    }
    _createdLocalBuffers.erase(name);
    {
        boost::unique_lock<boost::shared_mutex> lock(_localBufferMulticastAddressesMutex);
        _localBufferMulticastAddresses.erase(name);
    }
    scoped_lock<interprocess_upgradable_mutex> lock(*_localBufferMapLock);
    Buffer buf = (*_localBufferMap)[name];
    _segment.deallocate(_segment.get_address_from_handle(std::get<0>(buf)));
    _segment.deallocate(std::get<2>(buf).get());
    _localBufferMap->erase(name);
}

void dsm::Server::removeRemoteBuffer(std::string name, std::string ipaddr) {
    if (_createdRemoteBuffers.find(ipaddr+name) == _createdRemoteBuffers.end()) {
        return;
    }
    _createdRemoteBuffers.erase(ipaddr+name);
    scoped_lock<interprocess_upgradable_mutex> lock(*_localBufferMapLock);
    Buffer buf = (*_remoteBufferMap)[ipaddr+name];
    _segment.deallocate(_segment.get_address_from_handle(std::get<0>(buf)));
    _segment.deallocate(std::get<2>(buf).get());
    _remoteBufferMap->erase(name);
}

void dsm::Server::sendRequests() {
    //TODO could lock mutex, get values, then unlock
    boost::shared_lock<boost::shared_mutex> lock(_remoteBuffersToFetchMutex);
    for (auto const &i : _remoteBuffersToFetch) {
        std::cout << i.first << std::endl;
        boost::array<char, 28> sendBuffer;
        sendBuffer[0] = _portOffset;
        sendBuffer[1] = i.first.length();
        std::strcpy(&sendBuffer[2], i.first.c_str());
        _senderSocket->send_to(buffer(sendBuffer), i.second);
    }
}

void dsm::Server::sendACKs() {
    boost::upgrade_lock<boost::shared_mutex> ackLock(_remoteServersToACKMutex);
    for (auto &i : _remoteServersToACK) {
        std::cout << "ACK: " << i.first << std::endl;
        uint16_t len;
        {
            sharable_lock<interprocess_upgradable_mutex> mapLock(*_localBufferMapLock);
            len = std::get<1>((*_localBufferMap)[i.first]);
        }
        uint32_t multicastAddress;
        uint16_t multicastPort;
        {
            boost::shared_lock<boost::shared_mutex> multicastLock(_localBufferMulticastAddressesMutex);
            multicastAddress = _localBufferMulticastAddresses[i.first].address().to_v4().to_ulong();
            multicastPort = _localBufferMulticastAddresses[i.first].port();
        }
        std::cout << "ACKLEN: " << len << std::endl;
        boost::array<char, 36> sendBuffer;
        sendBuffer[0] = _portOffset;
        sendBuffer[0] |= 0x80;
        sendBuffer[1] = (uint8_t)i.first.length();
        memcpy(&sendBuffer[2], &len, sizeof(len));
        memcpy(&sendBuffer[4], &multicastAddress, sizeof(multicastAddress));
        memcpy(&sendBuffer[8], &multicastPort, sizeof(multicastPort));
        strcpy(&sendBuffer[10], i.first.c_str());
        for (auto const &j : i.second) {
            std::cout << "SENDING ACK: " << j.address() << " " << j.port() << std::endl;
            _senderSocket->send_to(buffer(sendBuffer), j);
        }
    }
    boost::upgrade_to_unique_lock<boost::shared_mutex> uniqueLock(ackLock);
    _remoteServersToACK.clear();
}

void dsm::Server::sendData() {
    boost::shared_lock<boost::shared_mutex> addressesLock(_localBufferMulticastAddressesMutex);
    sharable_lock<interprocess_upgradable_mutex> mapLock(*_localBufferMapLock);
    for (auto const &i : _localBufferMulticastAddresses) {
        std::cout << "SENDING DATA FOR " << i.first << std::endl;
        std::cout << "ENDPOINT IS " << i.second.address() << " " << i.second.port() << std::endl;
        Buffer buf = (*_localBufferMap)[i.first];
        void* data = _segment.get_address_from_handle(std::get<0>(buf));
        uint16_t len = std::get<1>(buf);
        sharable_lock<interprocess_upgradable_mutex> dataLock(*(std::get<2>(buf).get()));
        _senderSocket->send_to(buffer(data, len), i.second);
    }
}

void dsm::Server::processRequest(ip::udp::endpoint remoteEndpoint) {
    std::cout << "REQUEST" << std::endl;
    uint8_t len = (uint8_t)_receiveBuffer[1];
    std::string name(&_receiveBuffer[2], len);
    std::cout << name << std::endl;
    if (_createdLocalBuffers.find(name) != _createdLocalBuffers.end()) {
        std::cout << "HAS BUFFER" << std::endl;
        boost::unique_lock<boost::shared_mutex> lock(_remoteServersToACKMutex);
        remoteEndpoint.port(BASE_PORT+_receiveBuffer[0]);
        _remoteServersToACK[name].insert(remoteEndpoint);
    } else {
        std::cout << "NOT FOUND" << std::endl;
    }
}

void dsm::Server::processACK(ip::udp::endpoint remoteEndpoint) {
    std::cout << "RECEIVED ACK" << std::endl;
    std::string name(&_receiveBuffer[10], _receiveBuffer[1]);
    {
        //check if <name, addr, port> exists in remotes to create
        remoteEndpoint.port(BASE_PORT+(_receiveBuffer[0] & 0x0F));
        std::cout << "ACK NAME " << name << " " << remoteEndpoint.address().to_string() << " " << remoteEndpoint.port() << std::endl;
        if (_remoteBuffersToFetch.find(std::make_pair(name, remoteEndpoint)) == _remoteBuffersToFetch.end()) {
            std::cout << "ACK NAME DIDNT MATCH" << std::endl;
            return;
        }
        //delete entry if true and continue, else return
        boost::unique_lock<boost::shared_mutex> lock(_remoteBuffersToFetchMutex);
        std::cout << "SIZEBEFORE" << _remoteBuffersToFetch.size() << std::endl;
        _remoteBuffersToFetch.erase(std::make_pair(name, remoteEndpoint));
        std::cout << "SIZEAFTER" << _remoteBuffersToFetch.size() << std::endl;
    }
    {
        uint16_t buflen;
        memcpy(&buflen, &_receiveBuffer[2], sizeof(uint16_t));
        createRemoteBuffer(name, remoteEndpoint.address().to_string(), buflen);
    }
    {
        //create socket and start handler
        ip::udp::socket* sock = new ip::udp::socket(*_ioService);
        uint32_t mcastaddr;
        memcpy(&mcastaddr, &_receiveBuffer[4], sizeof(mcastaddr));
        uint16_t mcastport;
        memcpy(&mcastport, &_receiveBuffer[8], sizeof(mcastport));
        ip::udp::endpoint listenEndpoint(ip::address_v4::from_string("0.0.0.0"), mcastport);
        sock->open(ip::udp::v4());
        sock->set_option(ip::udp::socket::reuse_address(true));
        sock->bind(listenEndpoint);
        ip::address_v4 mcastv4(mcastaddr);
        std::cout << "ACK MULTICAST ADDR" << mcastv4.to_string() << std::endl;
        sock->set_option(ip::multicast::join_group(ip::address_v4(mcastaddr)));
        _remoteReceiveBuffers[remoteEndpoint.address().to_string()+name] = boost::array<char, 256>();
        ip::udp::endpoint senderEndpoint;
        sock->async_receive_from(buffer(_remoteReceiveBuffers[remoteEndpoint.address().to_string()+name]),
                                 senderEndpoint,
                                 boost::bind(&dsm::Server::processData,
                                             this,
                                             placeholders::error,
                                             placeholders::bytes_transferred,
                                             name,
                                             remoteEndpoint));
        _ioService->run();
        std::cout << "SOCKET CREATED + THREAD START" << std::endl;
        _sockets.push_back(sock);
    }
    uint16_t buflen;
    memcpy(&buflen, &_receiveBuffer[2], sizeof(uint16_t));
    std::cout << "NAME" << name << std::endl;
    std::cout << "BUFLEN" << buflen << std::endl;
    std::cout << "PORTOFFSET" << (_receiveBuffer[0] & 0x0F) << std::endl;
    struct in_addr addr;
    memcpy(&addr, &_receiveBuffer[4], 4);
    std::cout << "MCASTADDR" << inet_ntoa(addr) << std::endl;
    uint16_t mcastport;
    memcpy(&mcastport, &_receiveBuffer[8], 2);
    std::cout << "MCASTPORT" << mcastport << std::endl;
}

void dsm::Server::processData(const boost::system::error_code &error, size_t bytesReceived, std::string name, ip::udp::endpoint remoteEndpoint) {
    std::cout << "PROCESS DATA START" << std::endl;
    if (error) {
        std::cout << "ERROR" << std::endl;
        return;
    }
    std::string ipaddr = remoteEndpoint.address().to_string();
    sharable_lock<interprocess_upgradable_mutex> mapLock(*_remoteBufferMapLock);
    Buffer buf = (*_remoteBufferMap)[ipaddr+name];
    uint16_t len = std::get<1>(buf);
    if (len != bytesReceived) {
        std::cout << "LENGTHS NOT EQUAL" << std::endl;
        return;
    }
    std::cout << _remoteReceiveBuffers[ipaddr+name].data() << std::endl;
    /* void* ptr = _segment.get_address_from_handle(std::get<0>(buf)); */
    /* scoped_lock<interprocess_upgradable_mutex> dataLock(*(std::get<2>(buf).get())); */
    /* memcpy(ptr, _remoteReceiveBuffers[ipaddr+name].data(), len); */
}

void dsm::Server::senderThreadFunction() {
    while (_isRunning.load()) {
        std::cout << "SENDER" << std::endl;
        sendRequests();
        sendACKs();
        sendData();
        boost::this_thread::sleep_for(boost::chrono::seconds(3));
    }
}

void dsm::Server::receiverThreadFunction() {
    while (_isRunning.load()) {
        boost::system::error_code err;
        ip::udp::endpoint remoteEndpoint;
        _receiverSocket->receive_from(buffer(_receiveBuffer), remoteEndpoint, 0, err);

        std::cout << "RECEIVER" << std::endl;
        if (_receiveBuffer[0] == -1) {
            std::cout << "IGNORE PACKET" << std::endl;
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
