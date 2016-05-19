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
            Client(std::string name);
            virtual ~Client();

            bool registerLocalBuffer(std::string name, uint16_t length);
            bool registerRemoteBuffer(std::string name, std::string ipaddr);

            /* void getRemoteBufferContents(std::string name, Packet packet); */
            /* void setLocalBufferContents(std::string name, Packet packet); */
    };
}

#endif //DSMCLIENT_H
