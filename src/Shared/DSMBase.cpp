#include "DSMBase.h"

dsm::Base::Base(std::string name) : _name(name),
                                    _segment(interprocess::open_or_create, _name.c_str(), SEGMENT_SIZE),
                                    _messageQueue(interprocess::open_or_create, (name+"_queue").c_str(), MAX_NUM_MESSAGES, QUEUE_MESSAGE_SIZE)
{
    LocalBufferAllocator localBufferAllocator(_segment.get_segment_manager());
    _localBufferMap = _segment.find_or_construct<LocalBufferMap>("LocalBufferMap")(INITIAL_NUM_BUCKETS,
                                                                                   boost::hash<LocalBufferKey>(),
                                                                                   std::equal_to<LocalBufferKey>(),
                                                                                   localBufferAllocator);
    RemoteBufferAllocator remoteBufferAllocator(_segment.get_segment_manager());
    _remoteBufferMap = _segment.find_or_construct<RemoteBufferMap>("RemoteBufferMap")(INITIAL_NUM_BUCKETS,
                                                                                      boost::hash<RemoteBufferKey>(),
                                                                                      std::equal_to<RemoteBufferKey>(),
                                                                                      remoteBufferAllocator);

    _localBufferMapLock = _segment.find_or_construct<interprocess_sharable_mutex>("LocalBufferMapLock")();
    _remoteBufferMapLock = _segment.find_or_construct<interprocess_sharable_mutex>("RemoteBufferMapLock")();
}
