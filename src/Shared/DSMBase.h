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
#include <boost/interprocess/offset_ptr.hpp>
#include <boost/interprocess/sync/interprocess_upgradable_mutex.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>

using namespace boost::interprocess;

typedef std::tuple<managed_shared_memory::handle_t, int, offset_ptr<interprocess_upgradable_mutex>> Buffer;
typedef std::pair<const std::string, Buffer> MappedBuffer;
typedef allocator<MappedBuffer, managed_shared_memory::segment_manager> BufferAllocator;
typedef map<std::string, Buffer, std::less<std::string>, BufferAllocator> BufferMap;

/* For local buffers, this takes the form of <name, pass, length> */
typedef std::tuple<std::string, std::string, int> LocalBufferDefinition;
typedef allocator<LocalBufferDefinition, managed_shared_memory::segment_manager> LocalBufferDefinitionAllocator;
typedef vector<LocalBufferDefinition, LocalBufferDefinitionAllocator> LocalBufferDefinitionVector;

/* For remote buffers, this takes the form of <name, pass, ipaddr> */
typedef std::tuple<std::string, std::string, std::string> RemoteBufferDefinition;
typedef allocator<RemoteBufferDefinition, managed_shared_memory::segment_manager> RemoteBufferDefinitionAllocator;
typedef vector<RemoteBufferDefinition, RemoteBufferDefinitionAllocator> RemoteBufferDefinitionVector;

class DSMBase {
    public:
        DSMBase(std::string name);
        virtual ~DSMBase() = 0;
    protected:
        std::string _name;
        managed_shared_memory _segment;
        struct DSMLock {
            DSMLock() : isReady(false), localModified(false), remoteModified(false) {}
            bool isReady;
            bool localModified;
            bool remoteModified;
            boost::interprocess::interprocess_upgradable_mutex mutex;
            boost::interprocess::interprocess_condition ready;
        } *_lock;

        LocalBufferDefinitionVector *_localBufferDefinitions;
        RemoteBufferDefinitionVector *_remoteBufferDefinitions;

        BufferMap *_localBufferMap;
        BufferMap *_remoteBufferMap;
};

inline DSMBase::~DSMBase() {}

#endif //DSMBASE_H
