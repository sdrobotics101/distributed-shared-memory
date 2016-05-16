#include "../include/DSMServer.h"

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
    _segment.destroy<LocalBufferDefinitionVector>("LocalBufferDefinitionVector");
    _segment.destroy<RemoteBufferDefinitionVector>("RemoteBufferDefinitionVector");
    _segment.destroy<BufferMap>("LocalBufferMap");
    _segment.destroy<BufferMap>("RemoteBufferMap");
    _segment.destroy<DSMLock>("Lock");

    _ioService.stop();

    shared_memory_object::remove(_name.c_str());
}

void DSMServer::start() {
    scoped_lock<interprocess_mutex> lock(_lock->mutex);
    if (!_lock->isReady) {
        _lock->ready.wait(lock);
    }

    dump();

    allocateLocalBuffers();

    startReceive();
    _ioService.run();

    _lock->isReady = false;
    _lock->ready.notify_one();

    if (!_lock->isReady) {
        _lock->ready.wait(lock);
    }
}

/**
 * @brief should be removed at some point
 */
void DSMServer::dump() {
    std::cout << "Locals" << std::endl;
    for (int i = 0; i < (int)_localBufferDefinitions->size(); i++) {
        std::cout << std::get<0>((*_localBufferDefinitions)[i]) << " ";
        std::cout << std::get<1>((*_localBufferDefinitions)[i]) << " ";
        std::cout << std::get<2>((*_localBufferDefinitions)[i]) << std::endl;
    }
    std::cout << "Remotes" << std::endl;
    for (int i = 0; i < (int)_remoteBufferDefinitions->size(); i++) {
        std::cout << std::get<0>((*_remoteBufferDefinitions)[i]) << " ";
        std::cout << std::get<1>((*_remoteBufferDefinitions)[i]) << " ";
        std::cout << std::get<2>((*_remoteBufferDefinitions)[i]) << std::endl;
    }
}

void DSMServer::allocateLocalBuffers() {
    // should already be locked
    for (auto const &def : *_localBufferDefinitions) {
        void* buf = _segment.allocate(std::get<2>(def));
        managed_shared_memory::handle_t handle = _segment.get_handle_from_address(buf);
        offset_ptr<interprocess_upgradable_mutex> mutex = static_cast<interprocess_upgradable_mutex*>(_segment.allocate(sizeof(interprocess_upgradable_mutex)));
        new (mutex.get()) interprocess_upgradable_mutex;
        _localBufferMap->insert(std::make_pair(std::get<0>(def), std::make_tuple(handle, std::get<2>(def), mutex)));
    }
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
