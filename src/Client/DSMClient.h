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

#include "../Shared/DSMDefinitions.h"
#include "../Shared/DSMBase.h"

namespace dsm {
    class Client : public Base {
        public:
            Client(uint8_t serverID, uint8_t clientID, bool reset = true);
            virtual ~Client();

            bool registerLocalBuffer(std::string name, uint16_t length, bool localOnly);
            bool registerRemoteBuffer(std::string name, std::string ipaddr, uint8_t serverID);

            bool disconnectFromLocalBuffer(std::string name);
            bool disconnectFromRemoteBuffer(std::string name, std::string ipaddr, uint8_t serverID);

            uint16_t doesLocalExist(std::string name);
            uint16_t doesRemoteExist(std::string name, std::string ipaddr, uint8_t serverID);

            bool isRemoteActive(std::string name, std::string ipaddr, uint8_t serverID);

            bool getLocalBufferContents(std::string name, void* data);
            bool setLocalBufferContents(std::string name, const void* data);
            bool getRemoteBufferContents(std::string name, std::string ipaddr, uint8_t serverID, void* data);

#ifdef BUILD_PYTHON_MODULE
            std::string PY_getLocalBufferContents(std::string name);
            bool PY_setLocalBufferContents(std::string name, std::string data);
            std::string PY_getRemoteBufferContents(std::string name, std::string ipaddr, uint8_t serverID);
#endif
        private:
            uint8_t _clientID;
    };
}

#endif //DSMCLIENT_H
