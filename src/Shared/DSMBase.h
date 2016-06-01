#ifndef DSMBASE_H
#define DSMBASE_H

#include <string>
#include <cstdint>
#include <netinet/in.h>
#include <functional>

#include <boost/asio.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/interprocess_upgradable_mutex.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/functional/hash.hpp>

#include "DSMTypedefs.h"

#define SEGMENT_SIZE 65536
#define MAX_BUFFER_SIZE 1024

#define MAX_NUM_MESSAGES 10
#define MESSAGE_SIZE 32
#define INITIAL_NUM_BUCKETS 10

#define REQUEST_BASE_PORT 8888

//message type codes
#define CREATE_LOCAL 0
#define FETCH_REMOTE 1
#define CREATE_LOCALONLY 2
#define DISCONNECT_LOCAL 3
#define DISCONNECT_REMOTE 4
#define DISCONNECT_CLIENT 5

namespace interprocess = boost::interprocess;
namespace asio = boost::asio;
namespace ip = boost::asio::ip;

using interprocess::interprocess_upgradable_mutex;

namespace dsm {
    class Base {
        public:
            Base(std::string name);
            virtual ~Base() = 0;
        protected:
            std::string _name;
            interprocess::managed_shared_memory _segment;

            interprocess::message_queue _messageQueue;

            LocalBufferMap *_localBufferMap;
            RemoteBufferMap *_remoteBufferMap;
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
