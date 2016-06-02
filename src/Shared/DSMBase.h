#ifndef DSMBASE_H
#define DSMBASE_H

#include <string>
#include <cstdint>
#include <netinet/in.h>
#include <functional>

#include <boost/asio.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/interprocess_sharable_mutex.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/functional/hash.hpp>

#include "DSMTypedefs.h"

namespace interprocess = boost::interprocess;
namespace asio = boost::asio;
namespace ip = boost::asio::ip;

using interprocess::interprocess_sharable_mutex;

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
            interprocess_sharable_mutex* _localBufferMapLock;
            interprocess_sharable_mutex* _remoteBufferMapLock;

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
