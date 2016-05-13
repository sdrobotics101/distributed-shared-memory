#ifndef DSMCLIENT_H
#define DSMCLIENT_H

#include <string>
#include <tuple>

#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>

#include "Lock.h"

using namespace boost::interprocess;

typedef std::tuple<managed_shared_memory::handle_t, int> Buffer;
typedef allocator<Buffer, managed_shared_memory::segment_manager> SharedBufferAllocator;
typedef map<std::string, Buffer, SharedBufferAllocator> BufferMap;

typedef std::tuple<std::string, std::string, std::string> BufferDefinition;
typedef allocator<BufferDefinition, managed_shared_memory::segment_manager> SharedBufferDefinitionAllocator;
typedef vector<BufferDefinition, SharedBufferDefinitionAllocator> BufferDefinitionVector;


class DSMClient {
    public:
        DSMClient(std::string name);
        ~DSMClient();

        void initialize();
        void start();

        std::string registerLocalBuffer(std::string name, std::string ipaddr, std::string pass);
        std::string registerRemoteBuffer(std::string name, std::string ipaddr, std::string pass);

        /* void getRemoteBufferContents(std::string name, Packet packet); */
        /* void setLocalBufferContents(std::string name, Packet packet); */
    private:
        std::string _name;
        managed_shared_memory _segment;
        Lock *_lock;
        BufferDefinitionVector *_bufferDefinitions;
        BufferMap *_bufferMap;
};

#endif //DSMCLIENT_H
