#ifndef DSMCLIENT_H
#define DSMCLIENT_H

#include <set>
#include <iostream>
#include <thread>
#include <chrono>
#include <arpa/inet.h>

#include "../Shared/DSMBase.h"

namespace dsm {
    class Client : public Base {
        public:
            Client(std::string name, uint8_t clientID);
            virtual ~Client();

            bool registerLocalBuffer(std::string name, uint16_t length, bool localOnly);
            bool registerRemoteBuffer(std::string name, std::string ipaddr, uint8_t portOffset);

            bool disconnectFromLocalBuffer(std::string name);

            bool getRemoteBufferContents(std::string name, std::string ipaddr, void* data);
            bool getLocalBufferContents(std::string name, void* data);
            bool setLocalBufferContents(std::string name, const void* data);
        private:
            uint8_t _clientID;
    };
}

#endif //DSMCLIENT_H
