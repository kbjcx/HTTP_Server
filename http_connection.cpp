
#include "http_connection.h"

#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <cstdio>

int HTTPConnection::epoll_fd = -1;
int HTTPConnection::user_count = 0;

void set_no_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);
}

void addfd(int epoll_fd, int fd, bool one_shot) {
    epoll_event event{};
    event.data.fd = fd;
    // 监听读和异常断开
    event.events = EPOLLIN | EPOLLRDHUP;
    if (one_shot) {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
    // 设置文件描述符非阻塞
    set_no_blocking(fd);
}

void delfd(int epoll_fd, int fd) {
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
}

void modfd(int epoll_fd, int fd, uint32_t ev) {
    epoll_event event{};
    event.data.fd = fd;
    event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event);
}

HTTPConnection::HTTPConnection() = default;

HTTPConnection::~HTTPConnection() = default;

void HTTPConnection::init(int _fd, sockaddr_in& _addr) {
    sock_fd = _fd;
    addr = _addr;
    // 端口复用
    int reuse = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
    // 添加到epoll_fd中
    addfd(epoll_fd, sock_fd, true);
    ++user_count;
}

void HTTPConnection::close_connection() {
    if (sock_fd != -1) {
        delfd(epoll_fd, sock_fd);
        sock_fd = -1;
        --user_count;
    }
}

bool HTTPConnection::read() {
    printf("一次性睇完数据\n");
    return true;
}

bool HTTPConnection::write() {
    printf("一次性写完数据\n");
    return true;
}

void HTTPConnection::process() {
    // 交给线程池处理HTTP请求
    // 解析HTTP请求
    // 生成HTTP相应
    printf("parse request, create response\n");
}
