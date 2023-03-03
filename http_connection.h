#ifndef HTTP_SERVER_HTTPCONNECTION_H
#define HTTP_SERVER_HTTPCONNECTION_H

#include <arpa/inet.h>
#include <sys/socket.h>

#include <string>
#include <iostream>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <sys/uio.h>

class HTTPConnection {
public:
    enum Method { GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT };
    enum CheckState {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    enum LineStatus { LINE_OK = 0, LINE_BAD, LINE_OPEN };
    enum HttpCode {
        NO_REQUEST = 0,   // 还没解析完，需要继续解析客户端数据
        GET_REQUEST,      // 获得了一个完整的客户端请求
        BAD_REQUEST,      // 请求语法错误
        NO_RESOURCE,      // 服务器没有该资源
        FORBIDDEN_REQUEST,// 客户对资源没有足够的访问权限
        FILE_REQUEST,     // 文件请求并获取成功
        INTERNAL_ERROR,   // 服务器内部错误
        CLOSED_CONNECTION // 客户端已经关闭连接
    };
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
    CheckState check_state;
    int line_start; // 正在解析的行的起始位置
    int check_index; // 正在分析的字符在缓冲中的位置
    Method method;
    std::string url;
    std::string version;
    int content_length_;
    bool keep_alive_;
    std::string host_;
    std::string real_file_;
    struct stat file_stat_;
    // 内存映射首地址
    char* file_address_;
    // 读缓冲区当前位置
    int write_index;
    int bytes_to_send;
    int bytes_have_send;
    struct iovec io_vec_[2];
    int io_vec_count;
private:
    void init();
    void unmap();
    // 解析请求相关函数
    HttpCode parse_process(); // 解析请求
    HttpCode parse_request(const std::string& text); // 解析请求首行
    HttpCode parse_header(const std::string& text); // 解析请求头
    HttpCode parse_content(const std::string& text); // 解析请求体
    
    LineStatus parse_line(); // 获取一行的数据选择交给请求行、请求头还是请求体
    inline char* get_line();
    HttpCode do_request();
    // 响应请求相关函数
    bool response_process(HttpCode ret);
    bool add_response(const char* format, ...);
    bool add_status(int status, const char* title);
    bool add_headers(int content_length);
    bool add_content_length(int content_length);
    bool add_content_type();
    bool add_connection();
    bool add_blank_line();
    bool add_content(const char* content);
};

#endif
