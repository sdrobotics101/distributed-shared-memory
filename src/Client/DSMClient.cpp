#include "DSMClient.h"

DSMClient::DSMClient(std::string name) : DSMBase(name) {
    initialize();
    start();
}

DSMClient::~DSMClient() {

}

void DSMClient::initialize() {
    //this should read from a config file or something, but for testing, just set some bufs
    registerLocalBuffer("name0", 18);
    registerLocalBuffer("name0", 17);
    registerLocalBuffer("name1", 20);
    registerLocalBuffer("end", 30);
    /* registerRemoteBuffer("name0", "pass1", "ipaddr0"); */
    /* registerRemoteBuffer("name0", "pass1", "ipaddr0"); */
    /* registerRemoteBuffer("name1", "pass1", "ipaddr0"); */
}

void DSMClient::start() {
}

bool DSMClient::registerLocalBuffer(std::string name, uint16_t length) {
    if (sizeof(name.c_str()) > 26) {
        return false;
    }
    DSMMessage msg;
    msg.header = 0;
    std::strcpy(msg.name, name.c_str());
    msg.footer.size = length;

    _messageQueue.send(&msg, MESSAGE_SIZE, 0);
    return true;
}

/* std::string DSMClient::registerRemoteBuffer(std::string name, std::string pass, std::string ipaddr) { */
/*     scoped_lock<interprocess_upgradable_mutex> lock(_lock->mutex); */
/*     if (_remoteBufferNames.find(name) == _remoteBufferNames.end()) { */
/*         _remoteBufferNames.insert(name); */
/*         _remoteBufferDefinitions->push_back(std::make_tuple(name, pass, ipaddr)); */
/*         _lock->remoteModified = true; */
/*         return ipaddr+"_"+name; */
/*     } */
/*     return ""; */
/* } */
