#include "thread_pool.h"
#include <cstdio>

template <typename T>
ThreadPool<T>::ThreadPool(int _thread_num, int _max_request_num)
    : thread_num(_thread_num)
    , m_threads(nullptr)
    , max_request_num(_max_request_num)
    , stop(false) {
    if (thread_num <= 0 || max_request_num <= 0) {
        throw std::exception();
    }
    
    m_threads = new pthread_t[_thread_num];
    if (m_threads == nullptr) {
        throw std::exception();
    }
    
    // 创建线程
    for (int i = 0; i < thread_num; ++i) {
        printf("create the %dth thread", i);
        if (pthread_create(&m_threads[i], nullptr, worker, this) != 0) {
            delete[] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i]) != 0) {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template<typename T>
ThreadPool<T>::~ThreadPool() {
    delete[] m_threads;
    stop = true;
}

template<typename T>
bool ThreadPool<T>::append(T* request) {
    queue_locker.lock();
    if (work_queue.size() >= max_request_num) {
        queue_locker.unlock();
        return false;
    }
    work_queue.push_back(request);
    queue_locker.unlock();
    queue_stat.post();
    return true;
}

template<typename T>
void* ThreadPool<T>::worker(void* arg) {
    auto* thread_pool = (ThreadPool<T>*) arg;
    // 线程循环函数run()
    thread_pool->run();
}

template<typename T>
void ThreadPool<T>::run() {
    while (true) {
    
    }
}
