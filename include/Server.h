#include <string>
#include <tuple>

#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>

using namespace boost::interprocess;

typedef std::tuple<managed_shared_memory::handle_t, int> Buffer;
typedef allocator<Buffer, managed_shared_memory::segment_manager> SharedBufferAllocator;
typedef map<std::string, Buffer, SharedBufferAllocator> BufferMap;

typedef std::tuple<std::string, std::string, std::string> BufferDefinition;
typedef allocator<BufferDefinition, managed_shared_memory::segment_manager> SharedBufferDefinitionAllocator;
typedef vector<BufferDefinition, SharedBufferDefinitionAllocator> BufferDefinitionVector;

class DSMServer {
    public:
        DSMServer();
        ~DSMServer();

    private:
        BufferDefinitionVector *bufferDefinitions;
        BufferMap *bufferMap;
};
