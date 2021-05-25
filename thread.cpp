#include "uthreads.h"
#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <algorithm>
#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
		"rol    $0x11,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5 

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
		"rol    $0x9,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}

#endif

enum State {
    READY, BLOCKED, Running, DEAD
};

class Thread
{
private:
    int priority;
    int tid;
    char stack[STACK_SIZE];
    sigjmp_buf env;
    State state;
    int quanta_run;
public:
    Thread(int tid, void (*f)(void), State state);
    ~Thread();
    void setState(State st); // refactor to setState
    void addQuanta();
    int getqunta();
    sigjmp_buf * getEnv();
    State getState();
};
State Thread::getState()
{
    return this->state;
}
Thread::Thread(int tid, void (*f)(void), State state)
{
        this->tid = tid; 
        this->state = state;
        this->quanta_run = 0; 
        address_t sp, pc;
        sp = (address_t)stack + STACK_SIZE - sizeof(address_t);
        pc = (address_t)f;
        sigsetjmp(env, 1);
        (env->__jmpbuf)[JB_SP] = translate_address(sp);
        (env->__jmpbuf)[JB_PC] = translate_address(pc);
        if(sigemptyset(&env->__saved_mask))
        {
            fprintf(stderr,"ERROR\n");
            exit(1);
        }       
}
sigjmp_buf * Thread::getEnv()
{
    return &env;
}
Thread::~Thread()
{
    delete[] stack;
}
void Thread::setState(State st)
{
    this->state =st;
}
void Thread::addQuanta()
{
    this->quanta_run++;
}
int Thread::getqunta()
{
    return this->quanta_run;
}
