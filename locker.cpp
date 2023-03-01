#include <exception>
#include "locker.h"

Locker::Locker() {
    if(pthread_mutex_init(&m_mutex, nullptr) != 0) {
        throw std::exception();
    }
}

Locker::~Locker() {
    pthread_mutex_destroy(&m_mutex);
}

pthread_mutex_t* Locker::get() {
    return &m_mutex;
}

bool Locker::lock() {
    return pthread_mutex_lock(&m_mutex) == 0;
}

bool Locker::unlock() {
    return pthread_mutex_unlock(&m_mutex) == 0;
}

Condition::Condition() {
    if (pthread_cond_init(&m_cond, nullptr) != 0) {
        throw std::exception();
    }
}

Condition::~Condition() {
    pthread_cond_destroy(&m_cond);
}

bool Condition::wait(pthread_mutex_t* _m_mutex) {
    return pthread_cond_wait(&m_cond, _m_mutex) == 0;
}

bool Condition::timedwait(pthread_mutex_t* _m_mutex, struct timespec* t) {
    return pthread_cond_timedwait(&m_cond, _m_mutex, t) == 0;
}

bool Condition::signal() {
    return pthread_cond_signal(&m_cond) == 0;
}

bool Condition::broadcast() {
    return pthread_cond_broadcast(&m_cond) == 0;
}

Sema::Sema() {
    if (sem_init(&m_sem, 0, 0) != 0) {
        throw std::exception();
    }
}

Sema::Sema(int num) {
    if (sem_init(&m_sem, 0, num) != 0) {
        throw std::exception();
    }
}

Sema::~Sema() {
    sem_destroy(&m_sem);
}

bool Sema::wait() {
    return sem_wait(&m_sem) == 0;
}

bool Sema::post() {
    return sem_post(&m_sem) == 0;
}
