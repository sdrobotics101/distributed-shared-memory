#include "../include/DSMServer.h"

DSMServer::DSMServer(std::string name) : DSMBase(name) {}

DSMServer::~DSMServer() {
    for (auto const &i : *_localBufferMap) {
        _segment.deallocate(std::get<2>(i.second).get());
    }
    _segment.destroy<LocalBufferDefinitionVector>("LocalBufferDefinitionVector");
    _segment.destroy<RemoteBufferDefinitionVector>("RemoteBufferDefinitionVector");
    _segment.destroy<BufferMap>("LocalBufferMap");
    _segment.destroy<BufferMap>("RemoteBufferMap");
    _segment.destroy<DSMLock>("Lock");
    shared_memory_object::remove(_name.c_str());
}

void DSMServer::start() {
    scoped_lock<interprocess_mutex> lock(_lock->mutex);
    if (!_lock->isReady) {
        _lock->ready.wait(lock);
    }
    allocateBuffers();
    //TESTING CODE
    dump();
    char src[] = "adrenaline is pumping";
    managed_shared_memory::handle_t handle = std::get<0>(_localBufferMap->at("name0"));
    std::memcpy(_segment.get_address_from_handle(handle), src, std::get<1>(_localBufferMap->at("name0")));
    //END TESTING CODE
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

void DSMServer::allocateBuffers() {
    // should already be locked
    for (auto const &def : *_localBufferDefinitions) {
        void* buf = _segment.allocate(std::get<2>(def));
        managed_shared_memory::handle_t handle = _segment.get_handle_from_address(buf);
        offset_ptr<interprocess_mutex> mutex = static_cast<interprocess_mutex*>(_segment.allocate(sizeof(interprocess_mutex)));
        new (mutex.get()) interprocess_mutex;
        _localBufferMap->insert(std::make_pair(std::get<0>(def), std::make_tuple(handle, std::get<2>(def), mutex)));
    }
}
