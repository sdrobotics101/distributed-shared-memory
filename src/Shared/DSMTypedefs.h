#ifndef DSMTYPEDEFS_H
#define DSMTYPEDEFS_H

#include <string>
#include <tuple>
#include <cstdint>
#include <netinet/in.h>
#include <functional>
#include <exception>

#include <boost/asio.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/offset_ptr.hpp>
#include <boost/interprocess/sync/interprocess_upgradable_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>
#include <boost/interprocess/sync/upgradable_lock.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/unordered_map.hpp>
#include <boost/functional/hash.hpp>
#include <boost/scoped_ptr.hpp>

namespace interprocess = boost::interprocess;
namespace asio = boost::asio;
namespace ip = boost::asio::ip;

using interprocess::interprocess_upgradable_mutex;

struct RemoteBufferKey {
    RemoteBufferKey(std::string string, ip::udp::endpoint end) : name(string), endpoint(end) {}
    friend std::size_t hash_value(RemoteBufferKey const& e) {
        std::size_t seed = 0;
        boost::hash_combine(seed, e.name);
        boost::hash_combine(seed, e.endpoint.address().to_v4().to_ulong());
        boost::hash_combine(seed, e.endpoint.port());
        return seed;
    }
    friend bool operator==(const RemoteBufferKey& x, const RemoteBufferKey& y)  {
        return(x.name == y.name &&
               x.endpoint.address() == y.endpoint.address() &&
               x.endpoint.port() == y.endpoint.port());
    }
    friend std::ostream& operator<<(std::ostream& stream, const RemoteBufferKey& x) {
        stream << "{" << x.name << ", " << x.endpoint.address().to_v4().to_string() << ", " << x.endpoint.port() << "}";
        return stream;
    }
    const std::string name;
    const ip::udp::endpoint endpoint;
};

typedef std::tuple<interprocess::managed_shared_memory::handle_t, uint16_t, interprocess::offset_ptr<interprocess_upgradable_mutex>, ip::udp::endpoint> LocalBuffer;
typedef const std::string LocalBufferKey;
typedef std::pair<LocalBufferKey, LocalBuffer> MappedLocalBuffer;
typedef interprocess::allocator<MappedLocalBuffer, interprocess::managed_shared_memory::segment_manager> LocalBufferAllocator;
typedef boost::unordered_map<LocalBufferKey, LocalBuffer, boost::hash<LocalBufferKey>, std::equal_to<LocalBufferKey>, LocalBufferAllocator> LocalBufferMap;

typedef std::tuple<interprocess::managed_shared_memory::handle_t, uint16_t, interprocess::offset_ptr<interprocess_upgradable_mutex>> RemoteBuffer;
typedef std::pair<RemoteBufferKey, RemoteBuffer> MappedRemoteBuffer;
typedef interprocess::allocator<MappedRemoteBuffer, interprocess::managed_shared_memory::segment_manager> RemoteBufferAllocator;
typedef boost::unordered_map<RemoteBufferKey, RemoteBuffer, boost::hash<RemoteBufferKey>, std::equal_to<RemoteBufferKey>, RemoteBufferAllocator> RemoteBufferMap;

#endif //DSMTYPEDEFS_H
