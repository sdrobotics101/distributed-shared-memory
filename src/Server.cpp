#include "../include/Server.h"

DSMServer::DSMServer(std::string name) : _name(name),
                                         _segment(create_only, _name.c_str(), 65536),
                                         _sharedBufferDefinitionAllocator(_segment.get_segment_manager()),
                                         _sharedBufferAllocator(_segment.get_segment_manager())
{
    /* _lock = _segment.construct<Lock>("Lock")(); */
    _ready = _segment.construct<bool>("Ready")();
    _bufferDefinitions = _segment.construct<BufferDefinitionVector>("BufferDefinitionVector")(_sharedBufferDefinitionAllocator);
    _bufferMap = _segment.construct<BufferMap>("BufferMap")(_sharedBufferAllocator);
    *_ready = false;
}

DSMServer::~DSMServer() {
    /* _segment.destroy<Lock>("Lock"); */
    _segment.destroy<bool>("Ready");
    _segment.destroy<BufferDefinitionVector>("BufferDefinitionVector");
    _segment.destroy<BufferMap>("BufferMap");
    shared_memory_object::remove(_name.c_str());
}

void DSMServer::dump() {
    for (int i = 0; i < (int)_bufferDefinitions->size(); i++){
        std::cout << std::get<0>((*_bufferDefinitions)[i]) << std::endl;
    }
}
