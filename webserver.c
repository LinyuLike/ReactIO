#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/sendfile.h>
#include <errno.h>
#include <time.h>

#include "reactor.h"

#define WHOLE_HTML_RESPONSE     0
#define PART_HTML_RESPONSE      1

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

#if WHOLE_HTML_RESPONSE // 单次写入
    printf("Whole HTML Response\n");

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

#if PART_HTML_RESPONSE  // 多次写入
    printf("Part HTML Response, status:%d\n", c->status);

    int filefd = open("index.html", O_RDONLY);

    struct stat stat_buf;
    fstat(filefd, &stat_buf);

    if(c->status == 0){
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
        
        c->status = 1;  // 需要继续发送
    } else if (c->status == 1){
        // sendfile：
        // 参数1：目标文件描述符
        // 参数2：源文件描述符
        // 参数3：偏移量指针，NULL表示从当前位置开始
        // 参数4：要传输的字节数
        // sendfile的优势是传输高效，不需要经过用户缓冲区，直接在两个文件描述符之间传输数据
        // 最初只支持文件到socket的传输，后来逐渐增加对描述符类型的支持
        int ret = sendfile(c->fd, filefd, NULL, stat_buf.st_size);
        if(ret == 1){
            printf("errno: %d\n", errno);
        }
        c->status = 2; // 完成发送，需要清理
    } else if (c->status == 2){
        c->wlength = 0;
        memset(c->wbuffer, 0, BUFFER_LENGTH);
        c->status = 0; // 不需要额外处理
    }

    close(filefd);

#endif

    return 0;
}