#ifndef HTTP_SERVER_HTTPCONNECTION_H
#define HTTP_SERVER_HTTPCONNECTION_H

#include <arpa/inet.h>
#include <sys/socket.h>

class HTTPConnection {
public:
    // 所有的socket事件注册到同一个epoll_fd
    static int epoll_fd;
    static int user_count;
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;

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
    // 缓冲
    char read_buffer[READ_BUFFER_SIZE];
    char write_buffer[WRITE_BUFFER_SIZE];
    // 标识读缓冲区以及读入的客户端数据最后一个字节的下一个位置
    int read_index;
};

#endif
