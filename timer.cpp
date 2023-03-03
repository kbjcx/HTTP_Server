#include "timer.h"

SortTimerList::SortTimerList() : head(nullptr), tail(nullptr) {}

SortTimerList::~SortTimerList() {
    if (head != nullptr) {
        UtilTimer* tmp = head;
        while (head != nullptr) {
            head = head->next;
            delete tmp;
            tmp = head;
        }
    }
}

void SortTimerList::add_timer(UtilTimer* timer) {
    if (timer == nullptr) {
        return;
    }
    if (head == nullptr) {
        head = timer;
        tail = timer;
        return;
    }
    if (timer->expire_ <= head->expire_) {
        // 将timer放到头部
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }
    put(timer);
}

void SortTimerList::adjust_timer(UtilTimer* timer) {
    if (timer == nullptr) {
        return;
    }
    // 在尾部或者调整后依然小于下一个值，则不调整
    if (timer->next == nullptr || timer->next->expire_ >= timer->expire_) {
        return;
    }
    if (timer == head) {
        head = head->next;
        head->prev = nullptr;
        timer->next = nullptr;
        put(timer);
    }
    else {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        timer->prev = nullptr;
        timer->next = nullptr;
        put(timer);
    }
}

void SortTimerList::del_timer(UtilTimer* timer) {
    if (timer == nullptr) {
        return;
    }
    if (timer == head && timer == tail) {
        head = nullptr;
        tail = nullptr;
        delete timer;
        return;
    }
    if (timer == head) {
        head = head->next;
        head->prev = nullptr;
        delete timer;
        return;
    }
    if (timer == tail) {
        tail = tail->prev;
        tail->next = nullptr;
        delete timer;
        return ;
    }
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

void SortTimerList::put(UtilTimer* timer) {
    UtilTimer* tmp = head->next;
    while (tmp) {
        if (tmp->prev->expire_ < timer->expire_ && tmp->expire_ >= timer->expire_) {
            tmp->prev->next = timer;
            timer->prev = tmp->prev;
            tmp->prev = timer;
            timer->next = tmp;
            return ;
        }
        tmp = tmp->next;
    }
    tail->next = timer;
    timer->prev = tail;
    tail = timer;
    tail->next = nullptr;
}
