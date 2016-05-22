#include "DSMServer.h"

dsm::Server::Server(std::string name, uint8_t portOffset, std::string multicastAddress) : Base(name),
                                                                                          _isRunning(false),
                                                                                          _portOffset(portOffset),
                                                                                          _multicastAddress(multicastAddress)
{
    _ioService = new io_service();
    _senderSocket = new ip::udp::socket(*_ioService);
    _senderSocket->open(ip::udp::v4());
    _receiverSocket = new ip::udp::socket(*_ioService, ip::udp::endpoint(ip::udp::v4(), BASE_PORT+_portOffset));
}

dsm::Server::~Server() {
    std::cout << "DESTRUCTOR" << std::endl;
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
}

void dsm::Server::start() {
    //do some work to initialize network services, etc
    //create send and receive worker threads

    _senderThread = new boost::thread(boost::bind(&Server::senderThreadFunction, this));
    _receiverThread = new boost::thread(boost::bind(&Server::receiverThreadFunction, this));

    while(1) {
        unsigned int priority;
        message_queue::size_type receivedSize;
        _messageQueue.receive(&_message, MESSAGE_SIZE, receivedSize, priority);

        if(!_isRunning.load()) {
            std::cout << "BREAKING" << std::endl;
            break;
        }

        switch((_message.header & 0b11110000) >> 4) {
            case CREATE_LOCAL:
                std::cout << "LOCAL: " << _message.name << " " << _message.footer.size << " " << (_message.header & 0b00001111) << std::endl;
                createLocalBuffer(_message.name, _message.footer.size, _message.header);
                break;
            case CREATE_REMOTE:
                std::cout << "REMOTE: " << _message.name << " " << inet_ntoa(_message.footer.ipaddr) << " " << ((_message.header >> 8) & 0b00001111) << std::endl;
                createRemoteBuffer(_message.name, _message.footer.ipaddr, _message.header);
                break;
            case DISCONNECT_LOCAL:
                std::cout << "REMOVE LOCAL LISTENER: " << _message.name << " " << (_message.header & 0b00001111) << std::endl;
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
    _messageQueue.send(0, 0, 0);
}

void dsm::Server::createLocalBuffer(std::string name, uint16_t size, uint16_t header) {
    uint8_t clientID = header & 0b00001111;
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
    scoped_lock<interprocess_upgradable_mutex> lock(*_localBufferMapLock);
    _localBufferMap->insert(std::make_pair(name, std::make_tuple(handle, size, mutex)));
}

void dsm::Server::createRemoteBuffer(std::string name, struct in_addr addr, uint16_t header) {
    std::string ipaddr = inet_ntoa(addr);
    uint8_t portOffset = (header >> 8) & 0b00001111;
    _remoteBuffersToCreate.insert(std::make_pair(name, new ip::udp::endpoint(ip::address::from_string(ipaddr), BASE_PORT+portOffset)));
}

void dsm::Server::disconnectLocal(std::string name, uint16_t header) {
    _localBufferLocalListeners[name].erase(header & 0b00001111);
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
    scoped_lock<interprocess_upgradable_mutex> lock(*_localBufferMapLock);
    Buffer buf = (*_localBufferMap)[name];
    _segment.deallocate(_segment.get_address_from_handle(std::get<0>(buf)));
    _segment.deallocate(std::get<2>(buf).get());
    _localBufferMap->erase(name);
}

void dsm::Server::senderThreadFunction() {
    _isRunning = true;
    while(_isRunning.load()) {
        break;
        /* for (auto const &i : _remoteBuffersToCreate) { */
        /*     _senderSocket->send_to(buffer(i.first, i.first.length()), *(i.second)); */
        /* } */
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
    _isRunning = true;
    while(_isRunning.load()) {
        break;
        /* boost::system::error_code err; */
        /* ip::udp::endpoint remoteEndpoint; */
        /* _receiverSocket->receive_from(buffer(_receiveBuffer), remoteEndpoint, 0, err); */

        /* std::cout << _receiveBuffer.data() << std::endl; */
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
