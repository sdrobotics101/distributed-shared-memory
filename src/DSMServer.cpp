#include "../include/DSMServer.h"

DSMServer::DSMServer(std::string name) : DSMBase(name) {}

DSMServer::~DSMServer() {
    _segment.destroy<Lock>("Lock");
    _segment.destroy<BufferDefinitionVector>("BufferDefinitionVector");
    _segment.destroy<BufferMap>("BufferMap");
    shared_memory_object::remove(_name.c_str());
}

void DSMServer::start() {
    scoped_lock<interprocess_mutex> lock(_lock->mutex);
    if (!_lock->isReady) {
        _lock->ready.wait(lock);
    }
    dump();
}

/**
 * @brief should be removed at some point
 */
void DSMServer::dump() {
    for (int i = 0; i < (int)_bufferDefinitions->size(); i++) {
        std::cout << std::get<0>((*_bufferDefinitions)[i]) << " ";
        std::cout << std::get<1>((*_bufferDefinitions)[i]) << " ";
        std::cout << std::get<2>((*_bufferDefinitions)[i]) << std::endl;
    }
}
