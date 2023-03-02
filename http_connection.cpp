
#include "http_connection.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <cstdio>
#include <regex>

int HTTPConnection::epoll_fd = -1;
int HTTPConnection::user_count = 0;

bool ignore_case_compare(const std::string& str1, const std::string& str2) {
    if (str1.size() == str2.size()) {
        return std::equal(
            str1.begin(), str1.end(), str2.begin(),
            [](char a, char b) { return tolower(a) == tolower(b); });
    }
    else {
        return false;
    }
}

void set_no_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);
}

void addfd(int epoll_fd, int fd, bool one_shot, bool ET = true) {
    epoll_event event{};
    event.data.fd = fd;
    // 监听读和异常断开
    if (ET) {
        event.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
    }
    else {
        event.events = EPOLLIN | EPOLLRDHUP;
    }

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

void HTTPConnection::init() {
    check_state = CHECK_STATE_REQUESTLINE;
    line_start = 0;
    check_index = 0;
    method = GET;
    url = "";
    version = "";
}

void HTTPConnection::close_connection() {
    if (sock_fd != -1) {
        delfd(epoll_fd, sock_fd);
        sock_fd = -1;
        --user_count;
    }
}

bool HTTPConnection::read() {
    // printf("一次性睇完数据\n");
    // 缓冲区大小不够
    if (read_index >= READ_BUFFER_SIZE) {
        return false;
    }
    // 读取到的字节
    int read_bytes;
    while (true) {
        // 循环读取
        read_bytes = recv(sock_fd, read_buffer + read_index,
                          READ_BUFFER_SIZE - read_index, 0);
        if (read_bytes == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 没有数据
                break;
            }
            return false;
        }
        else if (read_bytes == 0) {
            // 对方关闭连接
            return false;
        }
        else {
            read_index += read_bytes;
        }
    }
    printf("读取到了数据: %s\n", read_buffer);
    return true;
}

bool HTTPConnection::write() {
    printf("一次性写完数据\n");
    return true;
}

void HTTPConnection::process() {
    // 交给线程池处理HTTP请求
    // 解析HTTP请求
    HttpCode ret = parse_process();
    if (ret == NO_REQUEST) {
        modfd(epoll_fd, sock_fd, EPOLLIN);
        return;
    }
    // 生成HTTP相应
    printf("parse request, create response\n");
}

HTTPConnection::HttpCode HTTPConnection::parse_process() {
    LineStatus line_status = LINE_OK;
    HttpCode ret = NO_REQUEST;

    char* text = nullptr;
    while ((line_status = parse_line()) == LINE_OK ||
           check_state == CHECK_STATE_CONTENT) {
        text = get_line();
        line_start = check_index;
        printf("got one http line: %s\n", text);
        switch (check_state) {
            case CHECK_STATE_REQUESTLINE: {
                ret = parse_request(text);
                if (ret == BAD_REQUEST) {
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADER: {
                ret = parse_header(text);
                if (ret == BAD_REQUEST) {
                    return BAD_REQUEST;
                }
                else if (ret == GET_REQUEST) {
                    do_request();
                }
            }
            case CHECK_STATE_CONTENT: {
                ret = parse_content(text);
                if (ret == GET_REQUEST) {
                    do_request();
                }
                line_status = LINE_OPEN;
                break;
            }
            default: {
                return INTERNAL_ERROR;
            }
        }
    }
    return NO_REQUEST;
}

char* HTTPConnection::get_line() {
    return read_buffer + line_start;
}

HTTPConnection::HttpCode HTTPConnection::do_request() {
}

HTTPConnection::LineStatus HTTPConnection::parse_line() {
    char temp;
    for (; check_index < read_index; ++check_index) {
        temp = read_buffer[check_index];
        if (temp == '\r') {
            if (check_index + 1 == read_index) {
                return LINE_OPEN;
            }
            else if (read_buffer[check_index + 1] == '\n') {
                read_buffer[check_index] = '\0';
                ++check_index;
                read_buffer[check_index] = '\0';
                ++check_index;
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if (temp == '\n') {
            if (check_index > 0 && read_buffer[check_index - 1] == '\r') {
                read_buffer[check_index - 1] = '\0';
                read_buffer[check_index] = '\0';
                ++check_index;
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

HTTPConnection::HttpCode HTTPConnection::parse_request(std::string text) {
    // GET /index.html HTTP/1.1
    std::regex reg("^([^\\s])*\\s([^\\s])*\\sHTTP/([^\\s])*");
    if (!std::regex_match(text, reg)) {
        return BAD_REQUEST;
    }
    // 判断开头方法是否为GET
    std::smatch result;
    if (std::regex_search(text, result, std::regex("^([^\\s])*"))) {
        if (!ignore_case_compare(result.str(), "GET")) {
            return BAD_REQUEST;
        }
        else {
            method = GET;
        }
    }
    else {
        return BAD_REQUEST;
    }
    std::cout << method << std::endl;
    // 获取url，条件是以/开始的后面紧跟空格的字符串，排除了版本号
    if (std::regex_search(text, result, std::regex("/([^\\s]*(?=\\s))"))) {
        url = result[0];
    }
    else {
        return BAD_REQUEST;
    }
    std::cout << url << std::endl;
    // 获取版本号，条件是以HTTP/开头的1.0或1.1
    // TODO 匹配其他版本号
    if (std::regex_search(text, result, std::regex("HTTP/1\\.[0|1]$"))) {
        version = result[0];
    }
    else {
        return BAD_REQUEST;
    }
    std::cout << version << std::endl;
    check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

HTTPConnection::HttpCode HTTPConnection::parse_header(char* text) {
    check_state = CHECK_STATE_CONTENT;
    return NO_REQUEST;
}

HTTPConnection::HttpCode HTTPConnection::parse_content(char* text) {
    return NO_REQUEST;
}
