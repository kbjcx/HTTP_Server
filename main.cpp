#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "http_connection.h"
#include "thread_pool.h"

#define THREAD_NUM 8
#define MAX_REQUEST_NUM 1024
#define MAX_FD 65535
#define MAX_EVENTS 10000

extern void addfd(int epoll_fd, int fd, bool one_shot, bool ET);
extern void delfd(int epoll_fd, int fd);

void add_sig(int sig, void (*handler)(int)) {
    struct sigaction sa;
    bzero(&sa, sizeof(sa));
    sa.sa_flags = 0;
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    sigaction(sig, &sa, nullptr);
}

int main(int argc, char* argv[]) {
    if (argc <= 1) {
        printf("Usage: %s Port\n", basename(argv[0]));
        exit(-1);
    }

    // 注册信号监听
    add_sig(SIGPIPE, SIG_IGN);

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

    // 添加文件描述符
    addfd(epoll_fd, server_sockfd, false, false);
    HTTPConnection::epoll_fd = epoll_fd;

    while (true) {
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
                users[client_fd].init(client_fd, client_addr);
            }
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                // 对方异常断开
                users[sock_fd].close_connection();
            }
            else if (events[i].events & EPOLLIN) {
                // 读事件
                if (users[sock_fd].read()) {
                    // 一次性读完数据
                    pool->append(users + sock_fd);
                }
                else {
                    users[sock_fd].close_connection();
                }
            }
            else if (events[i].events & EPOLLOUT) {
                // 写事件
                if (users[sock_fd].write()) {
                    // 一次性写完
                    continue;
                }
                else {
                    users[sock_fd].close_connection();
                }
            }
        }
    }
    close(epoll_fd);
    close(server_sockfd);
    delete[] users;
    delete pool;

    return 0;
}
