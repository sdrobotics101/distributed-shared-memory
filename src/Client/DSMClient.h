#ifndef DSMCLIENT_H
#define DSMCLIENT_H

#include <set>
#include <iostream>
#include <thread>
#include <chrono>
#include <arpa/inet.h>

#include "../Shared/DSMBase.h"

class DSMClient : public DSMBase {
    public:
        DSMClient(std::string name);
        virtual ~DSMClient();

        bool registerLocalBuffer(std::string name, uint16_t length);
        bool registerRemoteBuffer(std::string name, std::string ipaddr);

        /* void getRemoteBufferContents(std::string name, Packet packet); */
        /* void setLocalBufferContents(std::string name, Packet packet); */
};

#endif //DSMCLIENT_H
