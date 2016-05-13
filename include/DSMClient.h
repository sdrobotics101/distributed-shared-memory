#ifndef DSMCLIENT_H
#define DSMCLIENT_H

#include "DSMBase.h"

class DSMClient : public DSMBase {
    public:
        DSMClient(std::string name);
        virtual ~DSMClient();

        void initialize();
        void start();

        std::string registerLocalBuffer(std::string name, std::string ipaddr, std::string pass);
        std::string registerRemoteBuffer(std::string name, std::string ipaddr, std::string pass);

        /* void getRemoteBufferContents(std::string name, Packet packet); */
        /* void setLocalBufferContents(std::string name, Packet packet); */
};

#endif //DSMCLIENT_H
