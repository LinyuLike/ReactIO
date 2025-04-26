CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lpthread -lssl -lcrypto

TARGET = reactor_server
# 在这里添加新的源文件
SRCS = reactor.c webserver.c websocket.c
OBJS = $(SRCS:.c=.o)
# 在这里添加新的头文件
HEADERS = reactor.h

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean