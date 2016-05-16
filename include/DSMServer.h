#ifndef DSMSERVER_H
#define DSMSERVER_H

#include <iostream>

#include "DSMBase.h"

class DSMServer : public DSMBase {
    public:
        DSMServer(std::string name);
        virtual ~DSMServer();

        void start();

        void dump();
    private:
        void allocateBuffers();
};

#endif //DSMSERVER_H
