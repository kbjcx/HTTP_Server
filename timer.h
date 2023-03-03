#ifndef HTTP_SERVER_TIMER_H
#define HTTP_SERVER_TIMER_H

#include <arpa/inet.h>
#include <stdio.h>
#include <timer.h>

#define BUFFER_SIZE 64

class UtilTimer;//前向声明

struct ClientData {
    sockaddr_in addr;
    int sock_fd;
    char buffer[BUFFER_SIZE];
    UtilTimer* timer;
};

// 定时器类
class UtilTimer {
public:
    UtilTimer() : next(nullptr), prev(nullptr) {};

public:
    time_t expire_;// 任务超时时间
    // 任务回调函数，处理客户数据，有定时器的执行者传递给回调函数
    void (*callback)(ClientData*);
    ClientData* client_data_;
    UtilTimer* next;
    UtilTimer* prev;
};

class SortTimerList {
public:
    SortTimerList();
    ~SortTimerList();
    void add_timer(UtilTimer*);
    void adjust_timer(UtilTimer*);
    void del_timer(UtilTimer*);
private:
    void put(UtilTimer*);
    UtilTimer* head;
    UtilTimer* tail;
};

#endif
