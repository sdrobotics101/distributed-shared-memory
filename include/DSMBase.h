#ifndef DSMBASE_H
#define DSMBASE_H

#include <string>
#include <tuple>

#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>

#include "DSMLock.h"

using namespace boost::interprocess;

typedef std::tuple<managed_shared_memory::handle_t, int> Buffer;
typedef allocator<Buffer, managed_shared_memory::segment_manager> SharedBufferAllocator;
typedef map<std::string, Buffer, SharedBufferAllocator> BufferMap;

typedef std::tuple<std::string, std::string, std::string> BufferDefinition;
typedef allocator<BufferDefinition, managed_shared_memory::segment_manager> SharedBufferDefinitionAllocator;
typedef vector<BufferDefinition, SharedBufferDefinitionAllocator> BufferDefinitionVector;

class DSMBase {
    public:
        DSMBase(std::string name);
        virtual ~DSMBase() = 0;
    protected:
        std::string _name;
        managed_shared_memory _segment;
        Lock *_lock;
        BufferDefinitionVector *_bufferDefinitions;
        BufferMap *_bufferMap;
};

inline DSMBase::~DSMBase() {}

#endif //DSMBASE_H
