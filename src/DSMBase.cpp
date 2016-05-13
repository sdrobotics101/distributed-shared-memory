#include "../include/DSMBase.h"

DSMBase::DSMBase(std::string name) : _name(name),
                                     _segment(open_or_create, _name.c_str(), 65536)
{
    _lock = _segment.find_or_construct<Lock>("Lock")();

    SharedBufferDefinitionAllocator sharedBufferDefinitionAllocator(_segment.get_segment_manager());
    _bufferDefinitions = _segment.find_or_construct<BufferDefinitionVector>("BufferDefinitionVector")(sharedBufferDefinitionAllocator);

    SharedBufferAllocator sharedBufferAllocator(_segment.get_segment_manager());
    _bufferMap = _segment.find_or_construct<BufferMap>("BufferMap")(sharedBufferAllocator);
}
