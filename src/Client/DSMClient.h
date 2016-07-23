#ifndef DSMCLIENT_H
#define DSMCLIENT_H

#include <string>
#include <cstdint>

#include <boost/asio.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/interprocess_sharable_mutex.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>

#ifdef BUILD_PYTHON_MODULE
#include <boost/python.hpp>
#endif

#include "../Shared/DSMDefinitions.h"
#include "../Shared/DSMBase.h"

namespace dsm {
    class Client : public Base {
        public:
            Client(uint8_t serverID, uint8_t clientID, bool reset = true);
            virtual ~Client();

            static LocalBufferKey createLocalKey(std::string name);
            static RemoteBufferKey createRemoteKey(std::string name, std::string ipaddr, uint8_t serverID);

            bool registerLocalBuffer(LocalBufferKey key, uint16_t length, bool localOnly);
            bool registerRemoteBuffer(RemoteBufferKey key);

            bool disconnectFromLocalBuffer(LocalBufferKey key);
            bool disconnectFromRemoteBuffer(RemoteBufferKey key);

            uint16_t doesLocalExist(LocalBufferKey key);
            uint16_t doesRemoteExist(RemoteBufferKey key);

            bool isRemoteActive(RemoteBufferKey key);

            bool getLocalBufferContents(LocalBufferKey key, void* data);
            bool setLocalBufferContents(LocalBufferKey key, const void* data);

            uint8_t getRemoteBufferContents(RemoteBufferKey key, void* data);

#ifdef BUILD_PYTHON_MODULE
            bool PY_registerLocalBuffer(std::string name, uint16_t length, bool localOnly);
            bool PY_registerRemoteBuffer(std::string name, std::string ipaddr, uint8_t serverID);

            bool PY_disconnectFromLocalBuffer(std::string name);
            bool PY_disconnectFromRemoteBuffer(std::string name, std::string ipaddr, uint8_t serverID);

            uint16_t PY_doesLocalExist(std::string name);
            uint16_t PY_doesRemoteExist(std::string name, std::string ipaddr, uint8_t serverID);

            bool PY_isRemoteActive(std::string name, std::string ipaddr, uint8_t serverID);

            boost::python::list PY_getLocalBufferContents(std::string name);
            bool PY_setLocalBufferContents(std::string name, std::string data);

            boost::python::tuple PY_getRemoteBufferContents(std::string name, std::string ipaddr, uint8_t serverID);
#endif
        private:
            uint8_t _clientID;
    };
}

#endif //DSMCLIENT_H
