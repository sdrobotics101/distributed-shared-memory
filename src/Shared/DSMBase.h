#ifndef DSMBASE_H
#define DSMBASE_H

#include <string>
#include <tuple>
#include <cstdint>
#include <netinet/in.h>
#include <functional>
#include <exception>

#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/offset_ptr.hpp>
#include <boost/interprocess/sync/interprocess_upgradable_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/unordered_map.hpp>
#include <boost/functional/hash.hpp>
#include <boost/scoped_ptr.hpp>

#define SEGMENT_SIZE 65536
#define MAX_BUFFER_SIZE 1024

#define MAX_NUM_MESSAGES 10
#define MESSAGE_SIZE 32
#define INITIAL_NUM_BUCKETS 10

//message type codes
#define CREATE_LOCAL 0
#define CREATE_REMOTE 1
#define CREATE_LOCALONLY 2
#define DISCONNECT_LOCAL 3
#define DISCONNECT_REMOTE 4
#define DISCONNECT_CLIENT 5

namespace interprocess = boost::interprocess;
using interprocess::interprocess_upgradable_mutex;

typedef std::tuple<interprocess::managed_shared_memory::handle_t, uint16_t, interprocess::offset_ptr<interprocess_upgradable_mutex>> Buffer;
typedef std::pair<const std::string, Buffer> MappedBuffer;
typedef interprocess::allocator<MappedBuffer, interprocess::managed_shared_memory::segment_manager> BufferAllocator;
typedef boost::unordered_map<std::string, Buffer, boost::hash<std::string>, std::equal_to<std::string>, BufferAllocator> BufferMap;

namespace dsm {
    class Base {
        public:
            Base(std::string name);
            virtual ~Base() = 0;
        protected:
            std::string _name;
            interprocess::managed_shared_memory _segment;

            interprocess::message_queue _messageQueue;

            BufferMap *_localBufferMap;
            BufferMap *_remoteBufferMap;
            interprocess_upgradable_mutex* _localBufferMapLock;
            interprocess_upgradable_mutex* _remoteBufferMapLock;

            struct QueueMessage {
                uint16_t header;
                char name[26];      //makes this struct 32 bytes
                union footer {
                    uint16_t size;  //max buffer size will probably be smaller than max value of 16 bit int
                    struct in_addr ipaddr;
                } footer;
                void reset() {
                    header = 0;
                    strcpy(name, "");
                    footer.size = 0;
                }
            } _message;
    };
}

inline dsm::Base::~Base() {}

#endif //DSMBASE_H
