#include "DSMServer.h"

DSMServer::DSMServer(std::string name, int port) : DSMBase(name),
                                                   _ioService(),
                                                   _socket(_ioService, ip::udp::endpoint(ip::udp::v4(), port))
{
    start();
}

DSMServer::~DSMServer() {
    for (auto const &i : *_localBufferMap) {
        _segment.deallocate(std::get<2>(i.second).get());
    }
    _segment.destroy<BufferMap>("LocalBufferMap");
    _segment.destroy<BufferMap>("RemoteBufferMap");

    /* _ioService.stop(); */

    message_queue::remove((_name+"_queue").c_str());
    shared_memory_object::remove(_name.c_str());
}

void DSMServer::start() {
    //do some work to initialize network services, etc

    std::cout << sizeof(DSMMessage) << std::endl;
    while(1) {
        unsigned int priority;
        message_queue::size_type receivedSize;
        _messageQueue.receive(&_receivedMessage, MESSAGE_SIZE, receivedSize, priority);
        std::cout << _receivedMessage.name << " " << _receivedMessage.footer.size << std::endl;

        /* switch(_receivedMessage.header) { */
        /*     case 0: */
        /*         allocateLocalBuffer(_receivedMessage.name, _receivedMessage.footer.size); */
        /*         break; */
        /* } */

        if (strcmp(_receivedMessage.name, "end") == 0) {
            break;
        }
    }
}


void DSMServer::allocateLocalBuffer(std::string name, uint16_t size) {
    scoped_lock<interprocess_upgradable_mutex> lock(*_localBufferMapLock);
    if (_createdLocalBuffers.find(name) != _createdLocalBuffers.end()) {
        return;
    }
    //TODO make these 2 allocate calls into one
    void* buf = _segment.allocate(size);
    managed_shared_memory::handle_t handle = _segment.get_handle_from_address(buf);
    offset_ptr<interprocess_upgradable_mutex> mutex = static_cast<interprocess_upgradable_mutex*>(_segment.allocate(sizeof(interprocess_upgradable_mutex)));
    new (mutex.get()) interprocess_upgradable_mutex;
    _localBufferMap->insert(std::make_pair(name, std::make_tuple(handle, size, mutex)));
    _createdLocalBuffers.insert(name);
}

void DSMServer::startReceive() {
    _socket.async_receive_from(buffer(_receiveBuffer),
                               _endpoint,
                               boost::bind(&DSMServer::handleReceive,
                                           this,
                                           placeholders::error,
                                           placeholders::bytes_transferred));
}

void DSMServer::handleReceive(const boost::system::error_code& error, std::size_t bytesTransferred) {
    if (error) {
        return;
    }
    std::cout << std::string(_receiveBuffer.begin(), _receiveBuffer.begin()+bytesTransferred) << std::endl;
    startReceive();
}
