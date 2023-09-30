#include "simple_io.h"

#include <bits/stdc++.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>

int recv_all(int sockfd, void *buffer, size_t len)
{
    int rc;
    size_t bytes_received = 0;
    size_t bytes_remaining = len;
    char *buff = (char *) buffer;

    while (bytes_remaining)
    {
        rc = recv(sockfd, buff + bytes_received, bytes_remaining, 0);
        if (rc <= 0)
            return rc;

        bytes_received += rc;
        bytes_remaining -= rc;
    }

    return bytes_received;
}

int send_all(int sockfd, void *buffer, size_t len)
{
    int rc;
    size_t bytes_sent = 0;
    size_t bytes_remaining = len;
    char *buff = (char *) buffer;

    while(bytes_remaining)
    {
        rc = send(sockfd, buff + bytes_sent, bytes_remaining, 0);
        if (rc <= 0)
            return rc;

        bytes_sent += rc;
        bytes_remaining -= rc;
    }

    return bytes_sent;
}