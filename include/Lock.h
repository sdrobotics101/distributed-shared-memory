#ifndef LOCK_H
#define LOCK_H

#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>

struct Lock {
    Lock() : isReady(false) {}
    bool isReady;
    boost::interprocess::interprocess_mutex mutex;
    boost::interprocess::interprocess_condition ready;
};

#endif //LOCK_H
