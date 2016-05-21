#include "DSMBase.h"

dsm::Base::Base(std::string name) : _name(name),
                                    _segment(open_or_create, _name.c_str(), SEGMENT_SIZE),
                                    _messageQueue(open_or_create, (name+"_queue").c_str(), MAX_NUM_MESSAGES, MESSAGE_SIZE)
{
    BufferAllocator bufferAllocator(_segment.get_segment_manager());
    _localBufferMap = _segment.find_or_construct<BufferMap>("LocalBufferMap")(INITIAL_NUM_BUCKETS,
                                                                              boost::hash<std::string>(),
                                                                              std::equal_to<std::string>(),
                                                                              bufferAllocator);
    _remoteBufferMap = _segment.find_or_construct<BufferMap>("RemoteBufferMap")(INITIAL_NUM_BUCKETS,
                                                                                boost::hash<std::string>(),
                                                                                std::equal_to<std::string>(),
                                                                                bufferAllocator);

    _localBufferMapLock = _segment.find_or_construct<interprocess_upgradable_mutex>("LocalBufferMapLock")();
    _remoteBufferMapLock = _segment.find_or_construct<interprocess_upgradable_mutex>("RemoteBufferMapLock")();
}
