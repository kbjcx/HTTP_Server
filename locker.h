#ifndef HTTP_SERVER_LOCKER_H
#define HTTP_SERVER_LOCKER_H

#include <pthread.h>
#include <semaphore.h>

// 互斥锁类
class Locker {
private:
    pthread_mutex_t m_mutex;

public:
    Locker();
    ~Locker();

    bool lock();
    bool unlock();
    pthread_mutex_t* get();
};

// 条件信号
class Condition {
private:
    pthread_cond_t m_cond{};
public:
    Condition();
    ~Condition();
    
    bool wait(pthread_mutex_t* _m_mutex);
    bool timedwait(pthread_mutex_t* _m_mutex, struct timespec* t);
    bool signal();
    bool broadcast();
};

// 信号量
class Sema {
private:
    sem_t m_sem;
public:
    Sema();
    Sema(int num);
    ~Sema();
    
    bool wait();
    bool post();
};

#endif
