#include "DSMBase.h"

DSMBase::DSMBase(std::string name) : _name(name),
                                     _segment(open_or_create, _name.c_str(), 65536)
{
    _lock = _segment.find_or_construct<DSMLock>("Lock")();

    LocalBufferDefinitionAllocator localBufferDefinitionAllocator(_segment.get_segment_manager());
    _localBufferDefinitions = _segment.find_or_construct<LocalBufferDefinitionVector>("LocalBufferDefinitionVector")(localBufferDefinitionAllocator);

    RemoteBufferDefinitionAllocator remoteBufferDefinitionAllocator(_segment.get_segment_manager());
    _remoteBufferDefinitions = _segment.find_or_construct<RemoteBufferDefinitionVector>("RemoteBufferDefinitionVector")(remoteBufferDefinitionAllocator);

    BufferAllocator bufferAllocator(_segment.get_segment_manager());
    _localBufferMap = _segment.find_or_construct<BufferMap>("LocalBufferMap")(std::less<std::string>(), bufferAllocator);
    _remoteBufferMap = _segment.find_or_construct<BufferMap>("RemoteBufferMap")(std::less<std::string>(), bufferAllocator);
}
