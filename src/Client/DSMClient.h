#ifndef DSMCLIENT_H
#define DSMCLIENT_H

#include <set>
#include <iostream>
#include <thread>
#include <chrono>

#include "../Shared/DSMBase.h"

class DSMClient : public DSMBase {
    public:
        DSMClient(std::string name);
        virtual ~DSMClient();

        void initialize();
        void start();

        bool registerLocalBuffer(std::string name, std::string pass, int length);
        std::string registerRemoteBuffer(std::string name, std::string pass, std::string ipaddr);

        /* void getRemoteBufferContents(std::string name, Packet packet); */
        /* void setLocalBufferContents(std::string name, Packet packet); */
    private:
        std::set<std::string> _localBufferNames;
        std::set<std::string> _remoteBufferNames;
};

#endif //DSMCLIENT_H
