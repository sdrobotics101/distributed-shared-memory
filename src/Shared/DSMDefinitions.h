#ifndef DSMDEFINITIONS_H
#define DSMDEFINITIONS_H

#include <string>
#include <tuple>
#include <cstdint>
#include <functional>

#include <boost/asio.hpp>
#include <boost/unordered_map.hpp>
#include <boost/functional/hash.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/offset_ptr.hpp>
#include <boost/interprocess/sync/interprocess_sharable_mutex.hpp>
#include <boost/make_shared.hpp>

//This isn't actually enough for the max possible number of buffers
#define SEGMENT_SIZE 65536
#define MAX_BUFFER_SIZE 1024

#define MAX_NUM_MESSAGES 16
#define QUEUE_MESSAGE_SIZE 32
#define MAX_NAME_SIZE 25
#define INITIAL_NUM_BUCKETS 8

#define RECEIVER_BASE_PORT 8888
#define MULTICAST_BASE_PORT 30000

#define MAX_SERVERS 256
#define MAX_CLIENTS 256
#define MAX_BUFFERS_PER_CLIENT 8

#define SENDER_DELAY 20 //milliseconds
#define INACTIVITY_TIMEOUT 1000 //milliseconds

//message type codes
#define CREATE_LOCAL 0
#define FETCH_REMOTE 1
#define CREATE_LOCALONLY 2
#define DISCONNECT_LOCAL 3
#define DISCONNECT_REMOTE 4
#define DISCONNECT_CLIENT 5
#define CONNECT_CLIENT 6
#define CONNECT_CLIENT_NORESET 7

namespace interprocess = boost::interprocess;
namespace asio = boost::asio;
namespace ip = boost::asio::ip;

using interprocess::interprocess_sharable_mutex;

namespace dsm {
    typedef char BufferName[MAX_NAME_SIZE];

    struct LocalBufferKey {
        LocalBufferKey() : length(0) {
            std::strcpy(name, "");
        }
        LocalBufferKey(const LocalBufferKey &x) : length(x.length) {
            std::strcpy(name, x.name);
        }
        LocalBufferKey(const char* string) : length(strlen(string) > MAX_NAME_SIZE ? MAX_NAME_SIZE : strlen(string)) {
            std::strcpy(name, string);
        }
        LocalBufferKey(const char* string, uint8_t len) : length(len > MAX_NAME_SIZE ? MAX_NAME_SIZE : len) {
            std::strcpy(name, string);
        }
        friend std::size_t hash_value(LocalBufferKey const& e) {
            return boost::hash_range(e.name, e.name+e.length);
        }
        friend bool operator==(const LocalBufferKey& x, const LocalBufferKey& y)  {
            return !std::strcmp(x.name, y.name);
        }
        friend std::ostream& operator<<(std::ostream& stream, const LocalBufferKey& x) {
            stream << x.name;
            return stream;
        }
        LocalBufferKey& operator=(const LocalBufferKey &x) {
            std::strcpy(name, x.name);
            length = x.length;
            return *this;
        }
        BufferName name;
        uint8_t length;
    };

    struct RemoteBufferKey {
        RemoteBufferKey() : length(0) {
            std::strcpy(name, "");
        }
        RemoteBufferKey(const RemoteBufferKey &x) : length(x.length) {
            std::strcpy(name, x.name);
            endpoint = x.endpoint;
        }
        RemoteBufferKey(const char* string, ip::udp::endpoint end) : length(strlen(string) > MAX_NAME_SIZE ? MAX_NAME_SIZE : strlen(string)),
                                                                     endpoint(end) {
            std::strcpy(name, string);
        }
        RemoteBufferKey(const char* string, uint8_t len, ip::udp::endpoint end) : length(len > MAX_NAME_SIZE ? MAX_NAME_SIZE : len),
                                                                     endpoint(end) {
            std::strcpy(name, string);
        }
        friend std::size_t hash_value(RemoteBufferKey const& e) {
            std::size_t seed = boost::hash_range(e.name, e.name+e.length);
            boost::hash_combine(seed, e.endpoint.address().to_v4().to_ulong());
            boost::hash_combine(seed, e.endpoint.port());
            return seed;
        }
        friend bool operator==(const RemoteBufferKey& x, const RemoteBufferKey& y)  {
            return(!std::strcmp(x.name, y.name) &&
                   x.endpoint.address() == y.endpoint.address() &&
                   x.endpoint.port() == y.endpoint.port());
        }
        friend std::ostream& operator<<(std::ostream& stream, const RemoteBufferKey& x) {
            stream << "{" << x.name << ", " << x.endpoint.address().to_v4().to_string() << ", " << x.endpoint.port()-RECEIVER_BASE_PORT << "}";
            return stream;
        }
        RemoteBufferKey& operator=(const RemoteBufferKey &x) {
            std::strcpy(name, x.name);
            length = x.length;
            endpoint = x.endpoint;
            return *this;
        }
        BufferName name;
        uint8_t length;
        ip::udp::endpoint endpoint;
    };

    typedef std::tuple<interprocess::managed_shared_memory::handle_t, uint16_t, interprocess::offset_ptr<interprocess_sharable_mutex>, ip::udp::endpoint> LocalBuffer;
    typedef std::pair<LocalBufferKey, LocalBuffer> MappedLocalBuffer;
    typedef interprocess::allocator<MappedLocalBuffer, interprocess::managed_shared_memory::segment_manager> LocalBufferAllocator;
    typedef boost::unordered_map<LocalBufferKey, LocalBuffer, boost::hash<LocalBufferKey>, std::equal_to<LocalBufferKey>, LocalBufferAllocator> LocalBufferMap;

    typedef std::tuple<interprocess::managed_shared_memory::handle_t, uint16_t, interprocess::offset_ptr<interprocess_sharable_mutex>, bool> RemoteBuffer;
    typedef std::pair<RemoteBufferKey, RemoteBuffer> MappedRemoteBuffer;
    typedef interprocess::allocator<MappedRemoteBuffer, interprocess::managed_shared_memory::segment_manager> RemoteBufferAllocator;
    typedef boost::unordered_map<RemoteBufferKey, RemoteBuffer, boost::hash<RemoteBufferKey>, std::equal_to<RemoteBufferKey>, RemoteBufferAllocator> RemoteBufferMap;
}

#endif //DSMDEFINITIONS_H
