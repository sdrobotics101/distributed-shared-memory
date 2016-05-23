#include "DSMServer.h"

dsm::Server::Server(std::string name, uint8_t portOffset, std::string multicastAddress) : Base(name),
                                                                                          _isRunning(false),
                                                                                          _portOffset(portOffset),
                                                                                          _multicastAddress(ip::address::from_string(multicastAddress)),
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
    _senderThread->join();
    _receiverThread->join();
    delete _senderThread;
    delete _receiverThread;

    delete _senderSocket;
    delete _receiverSocket;
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
                createRemoteBuffer(_message.name, _message.footer.ipaddr, _message.header);
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
    //there is probably a better way to do this -- at least make it a set packet instead of empty
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

    _localBufferMulticastAddresses.insert(std::make_pair(name, ip::udp::endpoint(_multicastAddress, BASE_PORT+_multicastPortOffset)));
    _multicastPortOffset++;

    scoped_lock<interprocess_upgradable_mutex> lock(*_localBufferMapLock);
    _localBufferMap->insert(std::make_pair(name, std::make_tuple(handle, size, mutex)));
}

void dsm::Server::createRemoteBuffer(std::string name, struct in_addr addr, uint16_t header) {
    std::string ipaddr = inet_ntoa(addr);
    uint8_t portOffset = (header >> 8) & 0x0F;

    std::cout << "ENTER CREATE REMOTE" << std::endl;
    boost::unique_lock<boost::shared_mutex> lock(_remoteBuffersToCreateMutex);
    _remoteBuffersToCreate.insert(std::make_pair(name, ip::udp::endpoint(ip::address::from_string(ipaddr), BASE_PORT+portOffset)));
    std::cout << "EXIT CREATE REMOTE" << std::endl;
}

void dsm::Server::disconnectLocal(std::string name, uint16_t header) {
    _localBufferLocalListeners[name].erase(header & 0x0F);
    if (_localBufferLocalListeners[name].empty()) {
        std::cout << "REMOVING LOCAL BUFFER " << name << std::endl;
        removeLocalBuffer(name);
    }
}

void dsm::Server::removeLocalBuffer(std::string name) {
    if (_createdLocalBuffers.find(name) == _createdLocalBuffers.end()) {
        return;
    }
    _createdLocalBuffers.erase(name);
    _localBufferMulticastAddresses.erase(name);
    scoped_lock<interprocess_upgradable_mutex> lock(*_localBufferMapLock);
    Buffer buf = (*_localBufferMap)[name];
    _segment.deallocate(_segment.get_address_from_handle(std::get<0>(buf)));
    _segment.deallocate(std::get<2>(buf).get());
    _localBufferMap->erase(name);
}

void dsm::Server::senderThreadFunction() {
    while (_isRunning.load()) {
        std::cout << "SENDER" << std::endl;
        //TODO could lock mutex, get values, then unlock
        {
            boost::shared_lock<boost::shared_mutex> lock(_remoteBuffersToCreateMutex);
            for (auto const &i : _remoteBuffersToCreate) {
                std::cout << i.first << std::endl;
                boost::array<char, 28> sendBuffer;
                sendBuffer[0] = _portOffset;
                sendBuffer[1] = i.first.length();
                std::strcpy(&sendBuffer[2], i.first.c_str());
                _senderSocket->send_to(buffer(sendBuffer), i.second);
            }
        }
        {
            boost::upgrade_lock<boost::shared_mutex> lock(_remoteServersToACKMutex);
            for (auto &i : _remoteServersToACK) {
                std::cout << "ACK: " << i.first << std::endl;
                uint16_t len;
                {
                    sharable_lock<interprocess_upgradable_mutex> lock2(*_localBufferMapLock);
                    len = std::get<1>((*_localBufferMap)[i.first]);
                }
                std::cout << "ACKLEN: " << len << std::endl;
                boost::array<char, 30> sendBuffer;
                sendBuffer[0] = _portOffset;
                sendBuffer[0] |= 0x80;
                sendBuffer[1] = (uint8_t)i.first.length();
                memcpy(&sendBuffer[2], &len, 2);
                strcpy(&sendBuffer[4], i.first.c_str());
                for (auto const &j : i.second) {
                    std::cout << "SENDING ACK: " << j.address() << " " << j.port() << std::endl;
                    _senderSocket->send_to(buffer(sendBuffer), j);
                }
            }
            boost::upgrade_to_unique_lock<boost::shared_mutex> uniqueLock(lock);
            _remoteServersToACK.clear();
        }
        boost::this_thread::sleep_for(boost::chrono::seconds(3));
        //go through list of remotes that we want
        //construct and send packets to each of them
        //go through list of local buffers -> create packet for each
        //list of remote listeners ->send
        //go through list of local buffers to disconnect
        //send disconnect packets
        //check if there are any more listeners
        //remove if last
        //sleep
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

        if (_receiveBuffer[0] < 0) {
            std::cout << "RECEIVED ACK" << std::endl;
            std::string name(&_receiveBuffer[4], _receiveBuffer[1]);
            uint16_t buflen = (uint16_t)(_receiveBuffer[2]);
            std::cout << "STRLEN" << (uint8_t)(_receiveBuffer[1])+0 << std::endl;
            std::cout << "BUFLEN" << buflen << std::endl;
            std::cout << "NAME" << name << std::endl;
            std::cout << "PORTOFFSET" << (_receiveBuffer[0] & 0x0F) << std::endl;
            remoteEndpoint.port(BASE_PORT+(_receiveBuffer[0] & 0x0F));
            boost::unique_lock<boost::shared_mutex> lock(_remoteBuffersToCreateMutex);
            std::cout << "SIZEBEFORE" << _remoteBuffersToCreate.size() << std::endl;
            _remoteBuffersToCreate.erase(std::make_pair(name, remoteEndpoint));
            std::cout << "SIZEAFTER" << _remoteBuffersToCreate.size() << std::endl;
        }
        //block till we get a packet
        //if request
        //  check if we have the buffer, send ACK with buffer info if so
        //if ACK
        //  create a buffer with the specifications given and add it to structures
        //if data
        //  update already existing buffer
        //if disconnect
        //  go through steps to remove
        //  send ACK
        //if we want to disconnect and receive and ACK
        //  remove listener from list
        //  if last listener, delete buffer
    }
}
