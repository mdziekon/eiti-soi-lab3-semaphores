#ifndef SYNC_HPP
#define SYNC_HPP

#include <semaphore.h>

class Semaphore
{
private:
    sem_t sem;

public:
    Semaphore(int value);
    ~Semaphore();

    void wait();
    void post();
};

#endif