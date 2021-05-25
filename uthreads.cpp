#include "uthreads.h"
#include "thread.cpp"
#include <queue>
#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/time.h>
#include <algorithm>
#include <string.h>
#include <iostream>
using namespace std;
class Scheduler{
private:
    // 
    std::deque<int> ready_threads;
    //
    Thread *threads_arr[MAX_THREAD_NUM];
    //
    int cur_thread;
    //
    int num_quantums;
    //
    int quantumSec, quantomMSec;
    //
    static sigset_t schedulerSigSet;
    //
    struct sigaction sa;
    //
    struct itimerval timer;
    //
    /**
     * The function deletes the threads array 
     */
    void delete_threads_arr()
    {
        for(int i = 0; i < MAX_THREAD_NUM; i++)
        {
            if(threads_arr[i] != nullptr)
            {
                delete threads_arr[i];
                threads_arr[i] = nullptr;
            }
        }
    }
    /**
     * remove a tid from the ready qeue
     * @param tid the thread id 
     */
    void remove_from_ready_deque(const int tid)
    {
        this->ready_threads.erase(std::remove(this->ready_threads.begin(), this->ready_threads.end(), tid), this->ready_threads.end());
    }
    void run_algorithm()
    {

    }

public:
    static Scheduler *scheduler;
    /**
     * getter for current tid
     */
    int get_curent_tid()
    {
        return this->cur_thread;
    }
    /**
     * getter for num of quantums of a certain thread
     * @param tid the thread id 
     */
    int get_quantums(int tid)
    {
        if(tid >= MAX_THREAD_NUM || threads_arr[tid] == nullptr)
        {
            return functions_errors("Invalid TID");
        }
        return this->threads_arr[tid] ->getqunta();
    }
    /**
     * deals with system errors, frees the threads list and exits with 1
     * @param string the error message
     */
    void system_errors(const string &msg)
    {
        fprintf(stderr, "%s %s","system error:",msg);

        sigprocmask(SIG_UNBLOCK, &schedulerSigSet, nullptr);

        for(int i = 0; i < MAX_THREAD_NUM; i++)
        {
            if(threads_arr[i] != nullptr)
            {
                delete threads_arr[i];
                threads_arr[i] = nullptr;
            }
        }
        delete threads_arr;
        //this ->threads_arr = nullptr;
        exit(1);
    }

    /**
     * deals with functions errors, returns -1
     * @param string the error message
     */
    int functions_errors(const string msg)
    {
        fprintf(stderr, "%s %s","thread library error:",msg);
        sigprocmask(SIG_UNBLOCK, &schedulerSigSet, nullptr);
        return -1;
    }
    /**
     * This function blocks the time signal 
     */
    static void blockTimeSignal()
    {
        sigemptyset(&schedulerSigSet);
        sigaddset(&schedulerSigSet,SIGVTALRM);
        sigprocmask(SIG_BLOCK,&schedulerSigSet,nullptr);
    }
    /**
     * This function unblocks the time signal 
     */
    static void unblockTimeSignal()
    {
        sigemptyset(&schedulerSigSet);
        sigaddset(&schedulerSigSet,SIGVTALRM);
        sigprocmask(SIG_UNBLOCK,&schedulerSigSet,nullptr);
    }

    static void threadAction(int sig)
    {
        Scheduler::blockTimeSignal();
        Scheduler::scheduler->run_algorithm();
        Scheduler::unblockTimeSignal();
    }

    void startTimer()
    {
        this->timer.it_value.tv_sec = quantumSec;		// first time interval, seconds part
        this->timer.it_value.tv_usec = quantomMSec;		// first time interval, microseconds part

        this->timer.it_interval.tv_sec = quantumSec;	// following time intervals, seconds part
        this->timer.it_interval.tv_usec = quantomMSec;	// following time intervals, microseconds part

        // Start a virtual timer. It counts down whenever this process is executing.
        if (setitimer (ITIMER_VIRTUAL, &timer, NULL)) {
            this->system_errors("bad timer setting \n");
        }
    }

    int scheduler_block_thread(const int tid)
    {
        State s = this->threads_arr[tid]->getState();

        if (tid == 0)
        {
            return this->functions_errors("cannot block main thread\n");
        }
        if (tid >= MAX_THREAD_NUM || this->threads_arr[tid] == nullptr || tid < 0)
        {
            return this->functions_errors("Thread does not exist\n");
        }
        // TODO - deal with mutex

        if (s == READY)
        {
            this->remove_from_ready_deque(tid);
        }

        this->threads_arr[tid]->setState(BLOCKED);

        if (cur_thread == tid)
        {
            run_algorithm();
        }
        return 0;
    }
    int scheduler_resume_thread(int tid)
    {
        if(tid < 0 || tid > MAX_THREAD_NUM || this->threads_arr[tid] == nullptr)
        {
            return functions_errors("Invalid thread id");
        }

        State state = threads_arr[tid]->getState();

        if( state == Running || state == READY)
        {
            return 0;
        }

        threads_arr[tid]->setState(READY);

        //  todo IF IS NOT MUTEX THAN PUSH TO READY QUE

        return 0;
    }
    
    /**
     *
     * @param f
     * @return
     */
    int scheduler_spawn(void (*f)(void))
    {
        int id = -1;
        if(!f)
        {
            return functions_errors("function is null \n");
        }
        for(int i = 0; i < MAX_THREAD_NUM; i++)
        {
            if(this->threads_arr[i] == nullptr)
            {
                id = i;
                break;
            }
        }
        if(id == -1)  // No available space for another thread
        {
            return functions_errors("max number of threads reached \n");
        }

        try
        {
            Thread *thread = new Thread(id, f, READY);
            this->threads_arr[id] = thread;
            ready_threads.push_back(id);
        }
        catch(const std::exception &e)
        {
            system_errors("allocation or std error \n");
        }

        return id;
    }
    int get_num_of_quantums()
    {
        return this->num_quantums;
    }
    Scheduler(const int quantum_usecs)
    {
        this->num_quantums = 0;
        this->sa.sa_handler = &threadAction;
        if (sigaction(SIGVTALRM, &sa, nullptr) < 0) {
            system_errors("Could not create signal\n");
        }
        
        this->quantumSec = quantum_usecs / 1000000;
        this->quantomMSec = quantum_usecs % 1000000;
        for(auto &thrd: this->threads_arr)
        {
            thrd = nullptr;
        }
        try
        {
            this->threads_arr[0] = new Thread(0, nullptr, Running);
        }
        catch(const std::exception &e){
            system_errors("Error Maloc\n");        
        }

        cur_thread = 0; // refactor
        this->threads_arr[0]->addQuanta();
        startTimer();
    }

        /**
         * scheduler terminate function
         * @param tid thread id to terminate
         */
        int scheduler_terminate(int tid)
        {
            if(tid == 0)
            {
                delete_threads_arr();
                exit(0);
            }
            if(tid < 0 || tid > MAX_THREAD_NUM || this->threads_arr[tid] == nullptr)
            {
                return functions_errors("Thread id invalid \n");
            }

            // todo deal with mutex

            // todo if for mutex


            if(this->threads_arr[tid]->getState() == READY)
            {
                remove_from_ready_deque(tid);
            }

            delete this->threads_arr[tid];
            threads_arr[tid] = nullptr;

            if( cur_thread == tid)
            {
                run_algorithm();
            }

            return 0;
        }
    
};
Scheduler *Scheduler::scheduler;
/*
 * Description: This function initializes the thread library.
 * You may assume that this function is called before any other thread library
 * function, and that it is called exactly once. The input to the function is
 * the length of a quantum in micro-seconds. It is an error to call this
 * function with non-positive quantum_usecs.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_init(int quantum_usecs)
{
    Scheduler::blockTimeSignal();
    if(quantum_usecs < 1)
    {
        return Scheduler::scheduler->functions_errors("Num of Quantum should be lower than 1\n");
    }
    try {
        Scheduler::scheduler = new Scheduler(quantum_usecs);
    }catch (std::bad_alloc& ) {
            Scheduler::scheduler->system_errors("Error Maloc\n");   
    }
    Scheduler::unblockTimeSignal();
    return 0;
}

/*
 * Description: This function creates a new thread, whose entry point is the
 * function f with the signature void f(void). The thread is added to the end
 * of the READY threads list. The uthread_spawn function should fail if it
 * would cause the number of concurrent threads to exceed the limit
 * (MAX_THREAD_NUM). Each thread should be allocated with a stack of size
 * STACK_SIZE bytes.
 * Return value: On success, return the ID of the created thread.
 * On failure, return -1.
*/
int uthread_spawn(void (*f)(void))
{
    int new_thread_id;
    Scheduler::blockTimeSignal();
    new_thread_id = Scheduler::scheduler->scheduler_spawn(f);
    Scheduler::unblockTimeSignal();
    return new_thread_id;
}


/*
 * Description: This function terminates the thread with ID tid and deletes
 * it from all relevant control structures. All the resources allocated by
 * the library for this thread should be released. If no thread with ID tid
 * exists it is considered an error. Terminating the main thread
 * (tid == 0) will result in the termination of the entire process using
 * exit(0) [after releasing the assigned library memory].
 * Return value: The function returns 0 if the thread was successfully
 * terminated and -1 otherwise. If a thread terminates itself or the main
 * thread is terminated, the function does not return.
*/
int uthread_terminate(int tid)
{
    int ret;

    Scheduler::blockTimeSignal();

    ret = Scheduler::scheduler-> scheduler_terminate(tid);

    Scheduler::unblockTimeSignal();

    return ret;
}


/*
 * Description: This function blocks the thread with ID tid. The thread may
 * be resumed later using uthread_resume. If no thread with ID tid exists it
 * is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision
 * should be made. Blocking a thread in BLOCKED state has no
 * effect and is not considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_block(int tid)
{
    Scheduler::blockTimeSignal();

    Scheduler::scheduler->scheduler_block_thread(tid);

    Scheduler::unblockTimeSignal();

}


/*
 * Description: This function resumes a blocked thread with ID tid and moves
 * it to the READY state if it's not synced. Resuming a thread in a RUNNING or READY state
 * has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid)
{
    Scheduler::blockTimeSignal();
    Scheduler::scheduler->scheduler_resume_thread(tid);
    Scheduler::unblockTimeSignal();
}


/*
 * Description: This function tries to acquire a mutex. 
 * If the mutex is unlocked, it locks it and returns. 
 * If the mutex is already locked by different thread, the thread moves to BLOCK state. 
 * In the future when this thread will be back to RUNNING state, 
 * it will try again to acquire the mutex. 
 * If the mutex is already locked by this thread, it is considered an error. 
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_mutex_lock()
{
    Scheduler::blockTimeSignal();
    // code 
    Scheduler::unblockTimeSignal();
}


/*
 * Description: This function releases a mutex. 
 * If there are blocked threads waiting for this mutex, 
 * one of them (no matter which one) moves to READY state.
 * If the mutex is already unlocked, it is considered an error. 
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_mutex_unlock()
{
    Scheduler::blockTimeSignal();
    // code 
    Scheduler::unblockTimeSignal();
}


/*
 * Description: This function returns the thread ID of the calling thread.
 * Return value: The ID of the calling thread.
*/
int uthread_get_tid()
{
    return Scheduler::scheduler->get_curent_tid();
}


/*
 * Description: This function returns the total number of quantums since
 * the library was initialized, including the current quantum.
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number
 * should be increased by 1.
 * Return value: The total number of quantums.
*/
int uthread_get_total_quantums()
{
    return Scheduler::scheduler->get_num_of_quantums();
}


/*
 * Description: This function returns the number of quantums the thread with
 * ID tid was in RUNNING state. On the first time a thread runs, the function
 * should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state
 * when this function is called, include also the current quantum). If no
 * thread with ID tid exists it is considered an error.
 * Return value: On success, return the number of quantums of the thread with ID tid.
 * 			     On failure, return -1.
*/
int uthread_get_quantums(int tid)
{
    //do we need block? 
    return Scheduler::scheduler->get_quantums(tid);
}



