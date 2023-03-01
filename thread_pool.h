#ifndef HTTP_SERVER_THREAD_POOL_H
#define HTTP_SERVER_THREAD_POOL_H

#include <pthread.h>

#include <list>

#include "locker.h"

// 线程池
template <class T>
class ThreadPool {
private:
    // 线程的数量
    int thread_num;
    // 线程池数组
    pthread_t* m_threads;
    // 请求队列的最大数量
    int max_request_num;
    // 请求队列
    std::list<T*> work_queue;
    // 互斥锁
    Locker queue_locker;
    // 信号量，判断是否有任务需要处理
    Sema queue_stat;
    // 是否结束线程
    bool stop;

private:
    static void* worker(void* arg);
    void run();

public:
    ThreadPool(int _thread_num, int _max_request_num);
    ~ThreadPool();

    bool append(T* request);
};

#include <cstdio>

#include "thread_pool.h"

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
        printf("create the %dth thread\n", i);
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

template <typename T>
ThreadPool<T>::~ThreadPool() {
    delete[] m_threads;
    stop = true;
}

template <typename T>
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

template <typename T>
void* ThreadPool<T>::worker(void* arg) {
    auto* thread_pool = (ThreadPool<T>*) arg;
    // 线程循环函数run()
    thread_pool->run();
    return nullptr;
}

template <typename T>
void ThreadPool<T>::run() {
    while (!stop) {
        // 如果有信号量则正常执行，没有则阻塞
        // 不用信号量的话需要一直询问工作队列，造成资源浪费
        queue_stat.wait();
        queue_locker.lock();
        if (work_queue.empty()) {
            queue_locker.unlock();
            continue;
        }
        T* request = work_queue.front();
        work_queue.pop_front();
        queue_locker.unlock();

        if (request == nullptr) {
            continue;
        }
        request->process();
    }
}

#endif
