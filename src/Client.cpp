#include "../include/Client.h"

DSMClient::DSMClient(std::string name) : _name(name),
                                         _segment(open_only, _name.c_str())
{
    _lock = _segment.find<Lock>("Lock").first;
    _bufferDefinitions = _segment.find<BufferDefinitionVector>("BufferDefinitionVector").first;
    _bufferMap = _segment.find<BufferMap>("BufferMap").first;
    initialize();
    start();
}

DSMClient::~DSMClient() {

}

void DSMClient::initialize() {
    //this should read from a config file or something, but for testing, just set some bufs
    registerLocalBuffer("somename", "someaddr", "somepass");
    registerLocalBuffer("somename1", "someaddr1", "somepass1");
}

void DSMClient::start() {
    scoped_lock<interprocess_mutex> lock(_lock->mutex);
    _lock->isReady = true;
    _lock->ready.notify_one();
}

std::string DSMClient::registerLocalBuffer(std::string name, std::string ipaddr, std::string pass) {
    _bufferDefinitions->push_back(std::make_tuple(name, ipaddr, pass));
    return ipaddr+"_"+name;
}

std::string DSMClient::registerRemoteBuffer(std::string name, std::string ipaddr, std::string pass) {
    pass = pass; //Because unused warnings are unbearable
    return ipaddr+"_"+name;
}
