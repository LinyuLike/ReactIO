
#ifndef __REACTOR_H__
#define __REACTOR_H__

#define BUFFER_LENGTH   1024

typedef int (*RCALLBACK)(int fd); // 前缀R代表Reactor

struct conn {
    int fd;

    char rbuffer[BUFFER_LENGTH];
    int rlength;

    char wbuffer[BUFFER_LENGTH];
    int wlength;

    RCALLBACK send_callback; // 收到可写事件后执行的回调函数

    union 
    {
        RCALLBACK recv_callback;
        RCALLBACK accept_callback;
    } r_action;

    int status; // 多次收发用

    char *payload;        // 用于存储解码后的WebSocket数据
    char mask[4];         // 用于存储WebSocket帧的掩码
};

#endif