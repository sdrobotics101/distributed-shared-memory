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

            bool registerLocalBuffer(std::string name, uint16_t length);
            bool registerRemoteBuffer(std::string name, std::string ipaddr);

            bool disconnectFromBuffer(std::string name);

            /* void getRemoteBufferContents(std::string name, Packet packet); */
            /* void setLocalBufferContents(std::string name, Packet packet); */
        private:
            uint8_t _clientID;
    };
}

#endif //DSMCLIENT_H
