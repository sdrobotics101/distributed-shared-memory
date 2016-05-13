#include <string>

#include <boost/interprocess/managed_shared_memory.hpp>

using namespace std;
using namespace boost::interprocess;

class Packet;

class DSMClient {
    public:
        DSMClient();
        ~DSMClient();

        void start();

        string registerLocalBuffer(string name, string ipaddr, string pass);
        string registerRemoteBuffer(string name, string ipaddr, string pass);

        void getRemoteBufferContents(string name, Packet packet);
        void setLocalBufferContents(string name, Packet packet);
};
