#ifndef DSMCLIENT_H
#define DSMCLIENT_H

#include <string>
#include <cstdint>
#include <arpa/inet.h>

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/interprocess_sharable_mutex.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>

#include "../Shared/DSMTypedefs.h"
#include "../Shared/DSMBase.h"

namespace dsm {
    class Client : public Base {
        public:
            Client(uint8_t serverID, uint8_t clientID);
            virtual ~Client();

            bool registerLocalBuffer(std::string name, uint16_t length, bool localOnly);
            bool registerRemoteBuffer(std::string name, std::string ipaddr, uint8_t portOffset);

            bool disconnectFromLocalBuffer(std::string name);
            bool disconnectFromRemoteBuffer(std::string name, std::string ipaddr, uint8_t portOffset);

            bool doesLocalExist(std::string name);
            bool doesRemoteExist(std::string name, std::string ipaddr, uint8_t portOffset);

            bool getLocalBufferContents(std::string name, void* data);
            std::string getLocalBufferContents(std::string name);

            bool setLocalBufferContents(std::string name, const void* data);
            bool setLocalBufferContents(std::string name, std::string data);

            bool getRemoteBufferContents(std::string name, std::string ipaddr, uint8_t portOffset, void* data);
            std::string getRemoteBufferContents(std::string name, std::string ipaddr, uint8_t portOffset);
        private:
            uint8_t _clientID;
    };
}

#endif //DSMCLIENT_H
