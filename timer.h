#ifndef HTTP_SERVER_TIMER_H
#define HTTP_SERVER_TIMER_H

#include <arpa/inet.h>
#include <stdio.h>
#include <timer.h>
#include <ctime>
#include "http_connection.h"

class SortTimerList {
public:
    SortTimerList();
    ~SortTimerList();
    void add_timer(UtilTimer*);
    void adjust_timer(UtilTimer*);
    void del_timer(UtilTimer*);
    // 每次系统产生SIGALARM信号时都会执行tick处理到期任务
    void tick();
private:
    void put(UtilTimer*);
    UtilTimer* head;
    UtilTimer* tail;
};

#endif
