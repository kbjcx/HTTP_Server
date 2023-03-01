#ifndef HTTP_SERVER_THREAD_POOL_H
#define HTTP_SERVER_THREAD_POOL_H

#include <pthread.h>
#include <list>

#include "locker.h"

// 线程池
template<class T>
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

#endif
