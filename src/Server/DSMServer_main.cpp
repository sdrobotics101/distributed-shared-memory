#include <iostream>
#include <csignal>

#include <boost/program_options.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/scoped_ptr.hpp>

#include "DSMServer.h"

namespace po = boost::program_options;

boost::scoped_ptr<dsm::Server> _server;

void signalHandler(int) {
    _server->stop();
}

int main(int argc, char** argv) {
    try {
        int serverID;
        po::options_description generalOptions("Options");
        generalOptions.add_options()
            ("help,h", "print help message")
            ("remove,r", "destroy the memory segment associated with the serverID")
            ("force,f", "force creation of the server, implies -r")
            ("serverID,s", po::value<int>(&serverID), "the serverID of the server")
            ;
        po::positional_options_description positionalOptions;
        positionalOptions.add("serverID", 1);

        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).
                  options(generalOptions).
                  positional(positionalOptions).
                  run(), vm);
        po::notify(vm);

        if (vm.count("help") || !vm.count("serverID")) {
            std::cout << "Usage: DSMServer serverID [options]" << std::endl;
            std::cout << generalOptions;
            return 0;
        }

        if (serverID < 0 || serverID >= MAX_SERVERS) {
            std::cout << "Server ID must be between 0 and " << MAX_SERVERS-1 << std::endl;
            return 0;
        }

        std::string name = "server"+std::to_string(serverID);

        if (vm.count("force")) {
            std::cout << "forcing creation of ";
            interprocess::message_queue::remove((name+"_queue").c_str());
            boost::interprocess::shared_memory_object::remove(name.c_str());
        } else if (vm.count("remove")) {
            std::cout << "removing serverID " << serverID << std::endl;
            interprocess::message_queue::remove((name+"_queue").c_str());
            boost::interprocess::shared_memory_object::remove(name.c_str());
            return 0;
        }

        std::cout << "server ID: " << serverID << std::endl;

        _server.reset(new dsm::Server(serverID));
        std::signal(SIGINT, signalHandler);
        _server->start();
        return 0;
    } catch (std::exception& e) {
        std::cout << "Error: ";
        std::cout << e.what() << std::endl;
        return 1;
    }
}
