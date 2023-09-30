#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/timerfd.h>
#include <time.h>
#include <netinet/tcp.h>
#include "simple_io.h"

void start_server(int listenfd)
{
    int rc;
    int maxfd = 3;
    int client_count = 1;
    struct pollfd *fds = calloc(maxfd, sizeof(struct pollfd));

    fds[0].fd = listenfd;
    fds[0].events = POLLIN;

    rc = listen(listenfd, 25);
    if (rc < 0) {
        perror("listen");
        exit(1);
    }

    while (1) {
        rc = poll(fds, client_count, 0);
        if (rc < 0) {
            perror("poll");
            exit(1);
        }

        for (int i = 0; i < client_count; i++) {
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == listenfd) {
                    struct sockaddr_in client_addr;
                    socklen_t socket_len = sizeof(struct sockaddr_in);
                    int clientfd = accept(listenfd, (struct sockaddr *) &client_addr, &socket_len);
                    if (clientfd < 0) {
                        perror("accept");
                        exit(1);
                    }

                    if (client_count == maxfd) {
                        maxfd *= 2;
                        fds = realloc(fds, maxfd * sizeof(struct pollfd));
                    }

                    fds[client_count].fd = clientfd;
                    fds[client_count].events = POLLIN;
                    client_count++;

                    client_addr.
                }
            }
        }
    }
}

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    // setup server TCP socket
    int rc;
    int enable = 1;
    int port = atoi(argv[1]);
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);

    if (listenfd < 0) {
        perror("socket");
        exit(1);
    }

    // make address reusable
    rc = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    if (rc < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        exit(1);
    }

    // disable Nagle's algorithm
    rc = setsockopt(listenfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
    if (rc < 0) {
        perror("setsockopt(TCP_NODELAY) failed");
        exit(1);
    }

    struct sockaddr_in serv_addr;
    socklen_t socket_len = sizeof(struct sockaddr_in);

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    rc = bind(listenfd, (struct sockaddr *) &serv_addr, socket_len);
    if (rc < 0) {
        perror("bind");
        exit(1);
    }

    start_server(listenfd);

    close(listenfd);

    return 0;
}