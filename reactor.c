

#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <poll.h>
#include <sys/epoll.h>
#include <errno.h>
#include <sys/time.h>

#include "reactor.h"

#define CONNECTION_SIZE 1048576
#define MAX_PORTS       20

#define HTTP_MODE   0
#define WS_MODE     1

// 模式宏只能有一个为1
// 当 HTTP_MODE 设为 1，WS_MODE 设为 0 时，程序会编译和执行 HTTP 相关的代码
// 当 HTTP_MODE 设为 0，WS_MODE 设为 1 时，程序会编译和执行 WebSocket 相关的代码
#if (HTTP_MODE == 1 && WS_MODE == 1) || (HTTP_MODE == 0 && WS_MODE == 0)
#error "Invalid configuration: exactly one of HTTP_MODE or WS_MODE must be set to 1"
#endif

// 前置声明
int accept_cb(int fd);
int recv_cb(int fd);
int send_cb(int fd);

int http_request(struct conn *c);
int http_response(struct conn *c);

int ws_request(struct conn *c);
int ws_response(struct conn *c);


// 全局变量
int epfd = 0;
struct conn conn_list[CONNECTION_SIZE] = {0};


// 为一个fd在epoll中设置事件
// flag非零为增加，否则为更新
int set_event(int fd, int event, int flag){
    if(flag){
        struct epoll_event ev;
        ev.events = event;
        ev.data.fd = fd;
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
    } else {
        struct epoll_event ev;
        ev.events = event;
        ev.data.fd = fd;
        epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
    }
    return 0;
}

// 为一个client fd在epoll中注册event，并在数据数组中初始化
int event_register_client(int fd, int event){
    if(fd < 0) return -1;

    conn_list[fd].fd = fd;
    conn_list[fd].r_action.recv_callback = recv_cb;
    conn_list[fd].send_callback = send_cb;

    memset(conn_list[fd].rbuffer, 0, BUFFER_LENGTH);
    conn_list[fd].rlength = 0;

    memset(conn_list[fd].wbuffer, 0, BUFFER_LENGTH);
    conn_list[fd].wlength = 0;

    set_event(fd, event, 1);
    return 0;
}


int accept_cb(int fd){
    struct sockaddr_in clientaddr;
    socklen_t len = sizeof(clientaddr);

    int clientfd = accept(fd, (struct sockaddr*)&clientaddr, &len);
    if(clientfd < 0){
        printf("accept errno: %d --> %s\n", errno, strerror(errno));
		return -1;
    }

    event_register_client(clientfd, EPOLLIN);

    return 0;
}

int recv_cb(int fd){
    memset(conn_list[fd].rbuffer, 0, BUFFER_LENGTH);
    // note recv的最后一个参数：
    // 0 : 默认行为
    // MSG_PEEK：查看数据但不从接收队列中移除
    // MSG_WAITALL：等待直到接收到请求的字节数
    // MSG_DONTWAIT：非阻塞操作，即使socket设置为阻塞模式
    // MSG_OOB：处理带外数据
    int count = recv(fd, conn_list[fd].rbuffer, BUFFER_LENGTH, 0);

    // note recv的返回值:
    // 如果某个socket fd触发了可读事件，但是recv函数读取到的长度为0，通常表明连接已经被对方正常关闭了。
    // 因为在TCP协议中，对方调用close()时会发送一个FIN包，这会出发可读事件，
    // 但实际调用数据时，由于没有数据可读，且连接已关闭，因此会返回0。
    if(count == 0){
        printf("client disconnect: %d\n", fd);
        close(fd);
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
        return 0;
    } else if (count < 0){
        printf("count: %d, errno: %d, %s\n", count, errno, strerror(errno));
        close(fd);
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
        return 0;
    }

    conn_list[fd].rlength = count;

#if HTTP_MODE
    http_request(&conn_list[fd]);
#endif

#if WS_MODE
    ws_request(&conn_list[fd]);
#endif

    // 将监听事件更改为监听可写
    set_event(fd, EPOLLOUT, 0);

    return count;
}

int send_cb(int fd){
#if HTTP_MODE
    http_response(&conn_list[fd]);
#endif

#if WS_MODE
    ws_response(&conn_list[fd]);
#endif

    int count = 0;
    if (conn_list[fd].wlength != 0){
        count = send(fd, conn_list[fd].wbuffer, conn_list[fd].wlength, 0);
    }

    // 发送数据后，如果是http模式且状态为0，就切换回EPOLLIN
    // 如果是ws模式且状态为1，还切换回EPOLLIN
    // 这种差异是两种模式不同的state值代表的状态不同导致的
#if HTTP_MODE
    if(conn_list[fd].status == 0){
        set_event(fd, EPOLLIN, 0);
    }
#endif

#if WS_MODE
    if(conn_list[fd].status == 1 || conn_list[fd].status == 0){
        set_event(fd, EPOLLIN, 0);
    }
#endif

    
    return count;
}

int init_server(unsigned short port){
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in seraddr;
    seraddr.sin_family = AF_INET;
    seraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    seraddr.sin_port = htons(port);

    if(-1 == bind(sockfd, (struct sockaddr*)&seraddr, sizeof(struct sockaddr))){
        printf("bind failed: %s\n", strerror(errno));
    }

    listen(sockfd, 10);
    return sockfd;
}

int main(){
    unsigned  short port = 2000;

    epfd = epoll_create(1);

    int i = 0;
    for(i = 0; i < MAX_PORTS; i++){
        int sockfd = init_server(port + i);

        conn_list[sockfd].fd = sockfd;
        conn_list[sockfd].r_action.accept_callback = accept_cb;

        set_event(sockfd, EPOLLIN, 1);
    }

    // mainloop
    while(1){
        struct epoll_event events[1024] = {0};
        int nready = epoll_wait(epfd, events, 1024, -1);

        int i = 0;
        for(i = 0; i < nready; i++){
            int connfd = events[i].data.fd;

            if(events[i].events & EPOLLIN){
                conn_list[connfd].r_action.recv_callback(connfd);
            }
            if(events[i].events & EPOLLOUT){
                conn_list[connfd].send_callback(connfd);
            }
        }
    }
}