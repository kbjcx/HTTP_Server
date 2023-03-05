#include <arpa/inet.h>
#include <fcntl.h>
#include <csignal>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include "http_connection.h"
#include "thread_pool.h"
#include "timer.h"

#define THREAD_NUM 8
#define MAX_REQUEST_NUM 1024
#define MAX_FD 65535
#define MAX_EVENTS 10000

static int pipefd[2];
static SortTimerList timer_list;

extern void addfd(int epoll_fd, int fd, bool one_shot, bool ET);
extern void delfd(int epoll_fd, int fd);

void add_sig(int sig, void (*handler)(int), int restart) {
    struct sigaction sa;
    bzero(&sa, sizeof(sa));
    sa.sa_flags = 0;
    if (restart) {
        sa.sa_flags |= SA_RESTART;
    }
    sa.sa_handler = handler;
    // 信号处理时屏蔽所有信号
    sigfillset(&sa.sa_mask);
    sigaction(sig, &sa, nullptr);
}

// 可重入性：中断后再次进入该函数，环境变量与之前相同，不会丢失数据。
void sig_handler(int sig) {
    // Linux中系统调用的错误都存储于errno中，errno由操作系统维护，存储就近发生的错误。
    // 下一次的错误码会覆盖掉上一次的错误，为保证函数的可重入性，保留原来的errno
    int saved_errno = errno;
    int msg = sig;
    send(pipefd[1], (char*) &msg, 1, 0);
    errno = saved_errno;
}

void timer_handler() {
    timer_list.tick();
    alarm(TIMESLOT);
}

void callback(HTTPConnection* user) {
    user->close_connection();
}

int main(int argc, char* argv[]) {
    if (argc <= 1) {
        printf("Usage: %s Port\n", basename(argv[0]));
        exit(-1);
    }

    // 注册信号监听
    add_sig(SIGPIPE, SIG_IGN, false);
    add_sig(SIGALRM, sig_handler, true);
    add_sig(SIGTERM, sig_handler, true);

    // 创建线程池
    ThreadPool<HTTPConnection>* pool = nullptr;
    try {
        pool = new ThreadPool<HTTPConnection>(THREAD_NUM, MAX_REQUEST_NUM);
    }
    catch (...) {
        exit(-1);
    }

    // 保存客户端连接信息
    HTTPConnection* users = nullptr;
    users = new HTTPConnection[MAX_FD];

    int server_sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (server_sockfd == -1) {
        printf("socket error\n");
        exit(-1);
    }
    // 设置端口复用
    int reuse = 1;
    setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
    // 绑定文件描述符、监听地址和端口号
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(atoi(argv[1]));
    if (bind(server_sockfd, (struct sockaddr*) &server_addr,
             sizeof(server_addr)) == -1) {
        perror("bind error");
        exit(-1);
    }

    if (listen(server_sockfd, 5) == -1) {
        perror("listen error");
        exit(-1);
    }

    // 创建epoll对象,事件数组，添加删除修改文件描述符
    epoll_event events[MAX_EVENTS];
    int epoll_fd = epoll_create(1);

    // 创建信号通知管道
    assert(socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd) != -1);
    addfd(epoll_fd, pipefd[0], false, false);
    // 添加文件描述符
    addfd(epoll_fd, server_sockfd, false, false);
    HTTPConnection::epoll_fd = epoll_fd;

    bool timeout = false;
    bool stop_server = false;
    alarm(TIMESLOT);
    while (stop_server == false) {
        int count = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if ((count == -1) && (errno != EINTR)) {
            printf("epoll failure\n");
            break;
        }

        for (int i = 0; i < count; ++i) {
            int sock_fd = events[i].data.fd;
            if (sock_fd == server_sockfd) {
                // 有新客户端连接
                sockaddr_in client_addr{};
                socklen_t client_addr_len = sizeof(client_addr);
                int client_fd =
                    accept(server_sockfd, (struct sockaddr*) &client_addr,
                           &client_addr_len);
                if (client_fd == -1) {
                    perror("accept error");
                    continue;
                }

                if (HTTPConnection::user_count >= MAX_FD) {
                    // 目前连接数满了
                    // 回复相应的报文
                    close(client_fd);
                    continue;
                }
                // 新的客户初始化，放到数组中
                UtilTimer* timer = new UtilTimer();
                users[client_fd].init(client_fd, client_addr);
                users[client_fd].timer = timer;
                timer->init();
                timer->http_connection_ = &users[client_fd];
                timer->callback = callback;
                timer_list.add_timer(timer);
            }
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                // 对方异常断开
                timer_list.del_timer(users[sock_fd].timer);
                users[sock_fd].close_connection();
            }
            else if (events[i].events & EPOLLIN && sock_fd == pipefd[0]) {
                // 捕获到信号
                int ret;
                char signals[1024];
                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if (ret == -1) {
                    continue;
                }
                else if (ret == 0) {
                    continue;
                }
                else {
                    for (int i = 0; i < ret; ++i) {
                        switch (signals[i]) {
                            case SIGALRM: {
                                // 记录下有超时请求需要处理，但不立即处理，因为定时任务的优先级不高，需要优先处理其他事件
                                timeout = true;
                                break;
                            }
                            case SIGTERM: {
                                stop_server = true;
                                break;
                            }
                            default: {
                                break;
                            }
                        }
                    }
                }
            }
            else if (events[i].events & EPOLLIN) {
                // 读事件
                if (users[sock_fd].read()) {
                    // 一次性读完数据
                    pool->append(users + sock_fd);
                    time_t cur_time = time(nullptr);
                    users[sock_fd].timer->expire_ = cur_time + 3 * TIMESLOT;
                    printf("adjust time\n");
                    timer_list.adjust_timer(users[sock_fd].timer);
                }
                else {
                    timer_list.del_timer(users[sock_fd].timer);
                    users[sock_fd].close_connection();
                }
            }
            else if (events[i].events & EPOLLOUT) {
                // 写事件
                if (users[sock_fd].write()) {
                    // 一次性写完
                    time_t cur_time = time(nullptr);
                    users[sock_fd].timer->expire_ = cur_time + 3 * TIMESLOT;
                    printf("adjust time\n");
                    timer_list.adjust_timer(users[sock_fd].timer);
                    continue;
                }
                else {
                    users[sock_fd].close_connection();
                }
            }
        }
        if (timeout) {
            timer_handler();
            timeout = false;
        }
    }
    close(epoll_fd);
    close(server_sockfd);
    delete[] users;
    delete pool;

    return 0;
}
