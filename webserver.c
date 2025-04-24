#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/sendfile.h>
#include <errno.h>
#include <time.h>

#include "reactor.h"

#define WEBSERVER_ROOTDIR   "./"

#define WHOLE_HTML_RESPONSE   1

// 处理客户端请求的函数
int http_request(struct conn *c){
    // 实际上不管发送什么请求，都不会做出任何不同的操作
    memset(c->wbuffer, 0, BUFFER_LENGTH);
    c->wlength = 0;
    c->status = 0;
    return 0;
}

// 给客户端发送响应的函数
int http_response(struct conn *c){
#if WHOLE_HTML_RESPONSE

    int filefd = open("index.html", O_RDONLY);
    struct stat stat_buf;
    fstat(filefd, &stat_buf);

    // 获取当前时间
    time_t now = time(NULL);
    struct tm *time_info = gmtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%a, %d %b %Y %H:%M:%S GMT", time_info);

    c->wlength = sprintf(c->wbuffer,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Accept-Ranges: none\r\n"
        "Content-Length: %ld\r\n"
        "Date: %s\r\n\r\n",
        stat_buf.st_size,
        time_str);
    
    int count = read(filefd, c->wbuffer + c->wlength, BUFFER_LENGTH - c->wlength);
    c->wlength += count;

    close(filefd);

#endif

    return 0;
}