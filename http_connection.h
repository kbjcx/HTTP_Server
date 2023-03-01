#ifndef HTTP_SERVER_HTTPCONNECTION_H
#define HTTP_SERVER_HTTPCONNECTION_H

#include <sys/socket.h>
#include <arpa/inet.h>

class HTTPConnection {
public:
    // 所有的socket事件注册到同一个epoll_fd
    static int epoll_fd;
    static int user_count;
public:
    HTTPConnection();
    ~HTTPConnection();
    
    // 处理客户端请求
    void process();
    // 初始化
    void init(int _fd, sockaddr_in& _addr);
    void close_connection();
    bool read();
    bool write();
    
    
private:
    // http通信套接字
    int sock_fd{};
    // http通信地址
    sockaddr_in addr{};
};

#endif
