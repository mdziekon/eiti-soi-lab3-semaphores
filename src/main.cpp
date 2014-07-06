#include "sync.hpp"

#include <thread>
#include <stack>
#include <cassert>
#include <cmath>
#include <unistd.h>

#include <string>
#include <sstream>
#include <iostream>

#define COLOURS                 true

#define USLEEP_TIMEOUT          200

#define USLEEP_TIMEOUT_PROD_A   USLEEP_TIMEOUT
#define USLEEP_TIMEOUT_PROD_B   USLEEP_TIMEOUT
#define USLEEP_TIMEOUT_CONS_A   USLEEP_TIMEOUT
#define USLEEP_TIMEOUT_CONS_B   USLEEP_TIMEOUT

#define MAX_ELEMENT             8
#define MAX_STACK_SIZE          9

#define STACKSUM_LIMIT          20
#define STACKSIZE_LIMIT         3

#define MAX_ELEMENT_WIDTH       1 + log10(MAX_ELEMENT)
#define MAX_STACK_SIZE_WIDTH    1 + log10(MAX_STACK_SIZE)
#define STACKSUM_LIMIT_WIDTH    1 + log10(STACKSUM_LIMIT - 1 + MAX_ELEMENT)

template <class T>
class safeStack: public std::stack<T>
{
public:
    void push(const T &val)
    {
        assert(val > 0);
        assert(this->size() < MAX_STACK_SIZE);
        std::stack<T>::push(val);
    }

    void pop()
    {
        assert(!this->empty());
        std::stack<T>::pop();
    }
};

const std::string colourString(const std::string &str, const int &colourNo = 0, const int &width = 0)
{
    std::stringstream prepareStr;
    std::stringstream tmp;

    prepareStr.fill(' ');
    prepareStr.width(width);
    prepareStr << str;

    #if COLOURS
        tmp << "\033[" << colourNo << "m" << prepareStr.rdbuf() << "\033[0m";
        return tmp.str();
    #else
        return prepareStr.str();
    #endif
};

const std::string colourInt(const int &i, const int &colourNo = 0, const int &width = 0)
{
    std::stringstream prepareInt;
    std::stringstream tmp;

    prepareInt.fill(' ');
    prepareInt.width(width);
    prepareInt << i;

    #if COLOURS
        tmp << "\033[" << colourNo << "m" << prepareInt.rdbuf() << "\033[0m";
        return tmp.str();
    #else
        return prepareInt.str();
    #endif
}

class Speaker
{
protected:
    Semaphore mutex = Semaphore(1);

public:
    void say(const std::string &label, const std::string &text)
    {
        mutex.wait();
        std::cout << label << " " << text << ::std::endl;
        mutex.post();
    }
};

class Processes
{
protected:
    Speaker speaker;

    safeStack<unsigned int> stack;
    unsigned int stackSum       = 0;

    Semaphore   emptySem        = Semaphore(0),
                fullSem         = Semaphore(MAX_STACK_SIZE),
                rwMutex         = Semaphore(1),
                producerAMutex  = Semaphore(0),
                consumerBMutex  = Semaphore(0);

    bool    producerAIsWaiting  = false,
            consumerBIsWaiting  = false;

public:
    void sayProdA(const std::string &text)
    {
        speaker.say(colourString("[+] [PROD-A]", 31), text);
    }

    void sayProdB(const std::string &text)
    {
        speaker.say(colourString("[+] [PROD-B]", 32), text);
    }

    void sayConsA(const std::string &text)
    {
        speaker.say(colourString("[-] [CONS-A]", 33), text);
    }

    void sayConsB(const std::string &text)
    {
        speaker.say(colourString("[-] [CONS-B]", 34), text);
    }

    const std::string buildElementPushedInfo(const int &element)
    {
        std::stringstream builder;
        builder << "pushing element ("
                << colourInt(element, 36, MAX_ELEMENT_WIDTH)
                << ") on   stack (prev length: "
                << colourInt(stack.size(), 36, MAX_STACK_SIZE_WIDTH)
                << ", prev sum: "
                << colourInt(stackSum, 36, STACKSUM_LIMIT_WIDTH)
                << ", new sum: "
                << colourInt(stackSum + element, 36, STACKSUM_LIMIT_WIDTH)
                << ")";

        return builder.str();
    }

    const std::string buildElementPoppedInfo(const int &element)
    {
        std::stringstream builder;
        builder << "popping element ("
                << colourInt(element, 36, MAX_ELEMENT_WIDTH)
                << ") from stack (prev length: "
                << colourInt(stack.size() + 1, 36, MAX_STACK_SIZE_WIDTH)
                << ", prev sum: "
                << colourInt(stackSum, 36, STACKSUM_LIMIT_WIDTH)
                << ", new sum: "
                << colourInt(stackSum - element, 36, STACKSUM_LIMIT_WIDTH)
                << ")";

        return builder.str();
    }

    const std::string buildWaitMessage(const bool &isPush, const int &currentState, const int &requirement)
    {
        std::stringstream builder;
        builder << colourString("[---]", 91)
                << " can't " << (isPush ? "push" : "pop") << " the element"
                << " (" << (isPush ? "stack sum" : "stack size") << ": "
                << colourInt(currentState, 91)
                << " " << (isPush ? ">=" : "<=") << " "
                << colourInt(requirement, 36)
                << ")";

        return builder.str();
    }

    const std::string buildReleaseMessage(const bool &isPush, const int &currentState, const int &requirement)
    {
        std::stringstream builder;
        builder << colourString("[+++]", 92)
                << " " << (isPush ? "pushing" : "popping") << " after wait"
                << " (" << (isPush ? "stack sum" : "stack size") << ": "
                << colourInt(currentState, 92)
                << " " << (isPush ? "<" : ">") << " "
                << colourInt(requirement, 36)
                << ")";

        return builder.str();
    }

    void producerA()
    {
        unsigned int element;
        while (true)
        {
            element = rand() % MAX_ELEMENT + 1;

            fullSem.wait();

            rwMutex.wait();

            if (stackSum >= STACKSUM_LIMIT)
            {
                sayProdA(buildWaitMessage(true, stackSum, STACKSUM_LIMIT));

                producerAIsWaiting = true;

                rwMutex.post();
                producerAMutex.wait();
            }

            if (producerAIsWaiting)
            {
                sayProdA(buildReleaseMessage(true, stackSum, STACKSUM_LIMIT));

                producerAIsWaiting = false;
            }

            sayProdA(buildElementPushedInfo(element));

            assert(!(stackSum >= STACKSUM_LIMIT));
            stack.push(element);
            stackSum += element;

            emptySem.post();

            if (consumerBIsWaiting && stack.size() > STACKSIZE_LIMIT)
            {
                consumerBMutex.post();
            }
            else
            {
                rwMutex.post();
            }

            usleep(rand() % USLEEP_TIMEOUT_PROD_A + 1);
        }
    }

    void producerB()
    {
        unsigned int element;
        while (true)
        {
            element = rand() % MAX_ELEMENT + 1;

            fullSem.wait();

            rwMutex.wait();

            sayProdB(buildElementPushedInfo(element));

            stack.push(element);
            stackSum += element;

            emptySem.post();

            if (consumerBIsWaiting && stack.size() > STACKSIZE_LIMIT)
            {
                consumerBMutex.post();
            }
            else
            {
                rwMutex.post();
            }

            usleep(rand() % USLEEP_TIMEOUT_PROD_B + 1);
        }
    }

    void consumerA()
    {
        unsigned int element;
        while (true)
        {
            emptySem.wait();

            rwMutex.wait();

            element = stack.top();
            stack.pop();

            sayConsA(buildElementPoppedInfo(element));

            stackSum -= element;

            fullSem.post();

            if (producerAIsWaiting && stackSum < STACKSUM_LIMIT)
            {
                producerAMutex.post();
            }
            else
            {
                rwMutex.post();
            }

            usleep(rand() % USLEEP_TIMEOUT_CONS_A + 1);
        }
    }

    void consumerB()
    {
        unsigned int element;
        while (true)
        {
            emptySem.wait();

            rwMutex.wait();

            if (stack.size() <= STACKSIZE_LIMIT)
            {
                sayConsB(buildWaitMessage(false, stack.size(), STACKSIZE_LIMIT));

                consumerBIsWaiting = true;

                rwMutex.post();
                consumerBMutex.wait();
            }

            if (consumerBIsWaiting)
            {
                sayConsB(buildReleaseMessage(false, stack.size(), STACKSIZE_LIMIT));

                consumerBIsWaiting = false;
            }

            assert(!(stack.size() <= STACKSIZE_LIMIT));
            element = stack.top();
            stack.pop();

            sayConsB(buildElementPoppedInfo(element));

            stackSum -= element;

            fullSem.post();

            if (producerAIsWaiting && stackSum < STACKSUM_LIMIT)
            {
                producerAMutex.post();
            }
            else
            {
                rwMutex.post();
            }

            usleep(rand() % USLEEP_TIMEOUT_CONS_B + 1);
        }
    }
};

Processes proc;

void runProducerA()
{
    proc.producerA();
}
void runProducerB()
{
    proc.producerB();
}
void runConsumerA()
{
    proc.consumerA();
}
void runConsumerB()
{
    proc.consumerB();
}

int main (int argc, char** argv)
{
    srand(time(0));

    std::thread prodA(runProducerA);
    std::thread consA(runConsumerA);
    std::thread prodB(runProducerB);
    std::thread consB(runConsumerB);

    prodA.join();
    prodB.join();
    consA.join();
    consB.join();

    return 0;
}
