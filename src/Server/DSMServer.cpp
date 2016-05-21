#include "DSMServer.h"

dsm::Server::Server(std::string name, uint8_t portOffset) : Base(name),
                                                            _portOffset(portOffset) {
    _socket = new ip::udp::socket(_ioService, ip::udp::endpoint(ip::udp::v4(), BASE_PORT+_portOffset));
}

dsm::Server::~Server() {
    for (auto const &i : *_localBufferMap) {
        _segment.deallocate(_segment.get_address_from_handle(std::get<0>(i.second)));
        _segment.deallocate(std::get<2>(i.second).get());
    }
    _segment.destroy<BufferMap>("LocalBufferMap");
    _segment.destroy<BufferMap>("RemoteBufferMap");

    delete _socket;
    /* _ioService.stop(); */

    _isRunning = false;
    _senderThread->join();
    _receiverThread->join();
    delete _senderThread;
    delete _receiverThread;

    message_queue::remove((_name+"_queue").c_str());
    shared_memory_object::remove(_name.c_str());
}

void dsm::Server::start() {
    //do some work to initialize network services, etc
    //create send and receive worker threads

    _senderThread = new std::thread(&dsm::Server::senderThreadFunction, this);
    _receiverThread = new std::thread(&dsm::Server::receieverThreadFunction, this);

    while(1) {
        unsigned int priority;
        message_queue::size_type receivedSize;
        _messageQueue.receive(&_message, MESSAGE_SIZE, receivedSize, priority);
        switch((_message.header & 0b11110000) >> 4) {
            case CREATE_LOCAL:
                std::cout << "LOCAL: " << _message.name << " " << _message.footer.size << " " << (_message.header & 0b00001111) << std::endl;
                allocateLocalBuffer(_message.name, _message.footer.size);
                _localBufferLocalListeners[_message.name].insert(_message.header & 0b00001111);
                break;
            case CREATE_REMOTE:
                std::cout << "REMOTE: " << _message.name << " " << inet_ntoa(_message.footer.ipaddr) << std::endl;
                break;
            case DISCONNECT_LOCAL:
                std::cout << "REMOVE LOCAL LISTENER: " << _message.name << " " << (_message.header & 0b00001111) << std::endl;
                _localBufferLocalListeners[_message.name].erase(_message.header & 0b00001111);
                if (_localBufferLocalListeners[_message.name].empty()) {
                    std::cout << "REMOVING LOCAL BUFFER " << _message.name << std::endl;
                    removeLocalBuffer(_message.name);
                }
                break;
            default:
                std::cout << "UNKNOWN" << std::endl;
        }

        //this is just for convenience while testing
        if (strcmp(_message.name, "end") == 0) {
            _isRunning = false;
            break;
        }
        _message.reset();
    }
}


void dsm::Server::allocateLocalBuffer(std::string name, uint16_t size) {
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
    while(_isRunning) {
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

void dsm::Server::receieverThreadFunction() {
    _isRunning = true;
    while(_isRunning) {
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
