#include "../include/Server.h"

DSMServer::DSMServer(std::string name) : _name(name),
                                         _segment(open_or_create, _name.c_str(), 65536),
                                         _sharedBufferDefinitionAllocator(_segment.get_segment_manager()),
                                         _sharedBufferAllocator(_segment.get_segment_manager())
{
    _lock = _segment.find_or_construct<Lock>("Lock")();
    _bufferDefinitions = _segment.find_or_construct<BufferDefinitionVector>("BufferDefinitionVector")(_sharedBufferDefinitionAllocator);
    _bufferMap = _segment.find_or_construct<BufferMap>("BufferMap")(_sharedBufferAllocator);
}

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
