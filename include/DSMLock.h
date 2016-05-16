#ifndef DSMLOCK_H
#define DSMLOCK_H

#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>

struct DSMLock {
    DSMLock() : isReady(false) {}
    bool isReady;
    boost::interprocess::interprocess_mutex mutex;
    boost::interprocess::interprocess_condition ready;
};

#endif //DSMLOCK_H
