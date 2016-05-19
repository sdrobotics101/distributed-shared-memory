#include "DSMServer.h"

dsm::Server::Server(std::string name, int port) : Base(name),
                                                  _ioService(),
                                                  _socket(_ioService, ip::udp::endpoint(ip::udp::v4(), port))
{
    start();
}

dsm::Server::~Server() {
    for (auto const &i : *_localBufferMap) {
        _segment.deallocate(std::get<2>(i.second).get());
    }
    _segment.destroy<BufferMap>("LocalBufferMap");
    _segment.destroy<BufferMap>("RemoteBufferMap");

    /* _ioService.stop(); */

    message_queue::remove((_name+"_queue").c_str());
    shared_memory_object::remove(_name.c_str());
}

void dsm::Server::start() {
    //do some work to initialize network services, etc
    //create send and receive worker threads
    while(1) {
        unsigned int priority;
        message_queue::size_type receivedSize;
        _messageQueue.receive(&_message, MESSAGE_SIZE, receivedSize, priority);
        if (_message.header == 0) {
            std::cout << "LOCAL: " << _message.name << " " << _message.footer.size << std::endl;
        } else if (_message.header == 1) {
            std::cout << "REMOTE: " << _message.name << " " << inet_ntoa(_message.footer.ipaddr) << std::endl;
        } else {
            std::cout << "UNKNOWN" << std::endl;
        }

        /* switch(_message.header) { */
        /*     case 0: */
        /*         allocateLocalBuffer(_message.name, _message.footer.size); */
        /*         break; */
        /* } */

        if (strcmp(_message.name, "end") == 0) {
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

void dsm::Server::startReceive() {
    _socket.async_receive_from(buffer(_receiveBuffer),
                               _endpoint,
                               boost::bind(&dsm::Server::handleReceive,
                                           this,
                                           placeholders::error,
                                           placeholders::bytes_transferred));
}

void dsm::Server::handleReceive(const boost::system::error_code& error, std::size_t bytesTransferred) {
    if (error) {
        return;
    }
    std::cout << std::string(_receiveBuffer.begin(), _receiveBuffer.begin()+bytesTransferred) << std::endl;
    startReceive();
}
