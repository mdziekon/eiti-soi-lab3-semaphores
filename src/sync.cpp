#include "sync.hpp"

Semaphore::Semaphore(int value)
{
    if (sem_init(&sem, 0, value) != 0)
    {
        throw "Semaphore initialization failed!";
    }
}

Semaphore::~Semaphore()
{
    sem_destroy(&sem);
}

void Semaphore::wait()
{
    if (sem_wait(&sem) != 0)
    {
        throw "Semaphore wait() failed!";
    }
}

void Semaphore::post()
{
    if (sem_post(&sem) != 0)
    {
        throw "Semaphore post() failed!";
    }
}
