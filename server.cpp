#include <bits/stdc++.h>

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
#include "common.h"

using namespace std;

ofstream out;

void terminate_server(struct pollfd *fds, int client_count)
{
    close(fds[0].fd);
    close(fds[1].fd);

    for (int j = 3; j < client_count; j++)
        close(fds[j].fd);

    free(fds);
}

uint16_t get_content_length(char *content, uint8_t type)
{
    if (type == 0)
        return 5;
    else if (type == 1)
        return 2;
    else if (type == 2)
        return 6;
    else if (type == 3) {
        for (int i = 0; i < 1500; i++)
            if (content[i] == '\0')
                return i;

        return 1500;
    }

    return 0;
}

void start_server(int listenfd, int udp_fd)
{
    // general use buffer
    char buffer[BUFLEN];
    // error checking
    int rc;
    // maximum number of file descriptors that can fit into fds[]
    int maxfd = 3;
    int client_count = 3;
    struct pollfd *fds = (pollfd *) calloc(maxfd, sizeof(struct pollfd));
    // online client IDs
    vector<string> online;
    // file descriptor to ID mapping
    unordered_map<int, string> fd_to_ID;
    // topic to subscribers mapping
    unordered_map<string, vector<pair<int, int>>> topics;
    // ID to stored messages mapping
    unordered_map<string, vector<struct tcp_to_client_message>> stored_messages;

    if (fds == NULL) {
        terminate_server(fds, client_count);
        perror("calloc");
        exit(1);
    }

    // fd for receiving new TCP connection requests
    fds[0].fd = listenfd;
    fds[0].events = POLLIN;

    rc = listen(listenfd, 25);
    if (rc < 0) {
        terminate_server(fds, client_count);
        perror("listen");
        exit(1);
    }

    // fd for receiving new UDP packets
    fds[1].fd = udp_fd;
    fds[1].events = POLLIN;

    // fd for multiplexing stdin
    fds[2].fd = STDIN_FILENO;
    fds[2].events = POLLIN;

    // infinite loop for receiving new connections and messages
    while (1) {
        rc = poll(fds, client_count, -1);
        if (rc < 0) {
            terminate_server(fds, client_count);
            perror("poll");
            exit(1);
        }

        // check which file descriptors have data to be read
        for (int i = 0; i < client_count; i++) {
            if (fds[i].revents & POLLIN) {
                // TCP connection request
                if (fds[i].fd == listenfd) {
                    // accept new client
                    struct sockaddr_in client_addr;
                    socklen_t socket_len = sizeof(struct sockaddr_in);
                    int clientfd = accept(listenfd,
                                (struct sockaddr *) &client_addr, &socket_len);
                    if (clientfd < 0) {
                        terminate_server(fds, client_count);
                        perror("accept");
                        exit(1);
                    }

                    // check if we need to resize fds and realloc if so
                    if (client_count == maxfd) {
                        maxfd *= 2;
                        fds = (pollfd *) realloc(fds, maxfd * sizeof(struct pollfd));
                        if (fds == NULL) {
                            terminate_server(fds, client_count);
                            perror("realloc");
                            exit(1);
                        }
                    }

                    // add to fd array
                    fds[client_count].fd = clientfd;
                    fds[client_count].events = POLLIN;
                    client_count++;

                    // disable Nagle's algorithm
                    int flag = 1;
                    rc = setsockopt(clientfd, IPPROTO_TCP, TCP_NODELAY,
                                        (char *) &flag, sizeof(int));
                    if (rc < 0) {
                        terminate_server(fds, client_count);
                        perror("setsockopt");
                        exit(1);
                    }

                    // receive the ID of the client
                    memset(buffer, 0, BUFLEN);
                    rc = recv_all(clientfd, (void *) buffer, ID_LEN + 1);
                    if (rc < 0) {
                        terminate_server(fds, client_count);
                        perror("recv");
                        exit(1);
                    }

                    string ID(buffer);
                    bool already_connected = false;

                    // check if the client with the given ID is online
                    for (uint32_t j = 0; j < online.size(); j++) {
                            if (online[j].compare(ID) == 0) {
                                already_connected = true;
                                break;
                            }
                        }

                    /* if it is, print the corresponding message and close the
                       connection */
                    if (already_connected) {
                        printf("Client %s already connected.\n", buffer);
                        close(clientfd);
                        client_count--;
                    } else {
                        /* otherwise, add it to the list of online clients and
                           print corresponding message */
                        printf("New client %s connected from %s:%d.\n", buffer,
                        inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                        online.push_back(ID);
                        fd_to_ID[clientfd] = ID;

                        /* send stored messages for the client's subscribed
                           topics if they exist */
                        if (stored_messages.find(ID) !=
                                stored_messages.end()) {
                            for (uint32_t j = 0;
                                    j < stored_messages[ID].size(); j++) {
                                struct tcp_to_client_message msg;
                                msg = stored_messages[ID][j];
                                uint16_t content_length = get_content_length
                                                            (msg.content, msg.type);

                                uint16_t network_len = htons(content_length);

                                rc = send_all(clientfd, (void *) &network_len,
                                            sizeof(uint16_t));
                                if (rc < 0) {
                                    terminate_server(fds, client_count);
                                    perror("send");
                                    exit(1);
                                }

                                rc = send_all(clientfd, (void *) &msg,
                                            CLIENT_MESSAGE_SIZE_NO_CONTENT +
                                            content_length);
                                if (rc < 0) {
                                    terminate_server(fds, client_count);
                                    perror("send");
                                    exit(1);
                                }
                            }

                            while (!stored_messages[ID].empty())
                                stored_messages[ID].pop_back();
                        }
                    }
                } else if (fds[i].fd == STDIN_FILENO) {
                    // the server has some input to read from stdin
                    char *ret = fgets(buffer, sizeof(buffer), stdin);
                    if (ret == NULL) {
                        terminate_server(fds, client_count);
                        perror("fgets");
                        exit(1);
                    }

                    /* stop the server if the user typed "exit", otherwise
                       log an error message for invalid input, as there's no
                       other valid input */
                    if (strcmp(buffer, "exit\n") == 0) {
                        // close all connections and stop the server
                        close(listenfd);
                        close(udp_fd);

                        for (int j = 3; j < client_count; j++)
                            close(fds[j].fd);

                        free(fds);

                        exit(0);
                    } else
                        out << "Invalid command.\n";
                } else if (fds[i].fd == udp_fd) {
                    // there's a UDP message to be read
                    struct sockaddr_in client_addr;
                    socklen_t socket_len = sizeof(struct sockaddr_in);

                    memset(buffer, 0, BUFLEN);
                    rc = recvfrom(udp_fd, (void *) buffer,
                                sizeof(struct udp_message), 0,
                                (struct sockaddr *) &client_addr, &socket_len);
                    if (rc < 0) {
                        terminate_server(fds, client_count);
                        perror("recvfrom");
                        exit(1);
                    }

                    /* parse message and convert it to structure to be sent to
                       all subscribed TCP clients */
                    struct udp_message *msg = (struct udp_message *) buffer;
                    struct tcp_to_client_message tcp_msg;

                    memset(&tcp_msg, 0, sizeof(struct tcp_to_client_message));
                    tcp_msg.sender_IP = client_addr.sin_addr.s_addr;
                    tcp_msg.sender_port = client_addr.sin_port;
                    memcpy(tcp_msg.topic, msg->topic, 50);
                    memcpy(&tcp_msg.type, &msg->type, 1);
                    memcpy(tcp_msg.content, msg->content, 1500);

                    char topicChar[51];

                    memset(topicChar, 0, 51);
                    memcpy(topicChar, msg->topic, 50);
                    topicChar[50] = '\0';

                    uint16_t content_length = get_content_length
                                                (tcp_msg.content, msg->type);
                    uint16_t network_length = htons(content_length);
                    string topic(topicChar);

                    // send message to all subscribers
                    if (topics.find(topic) != topics.end() &&
                            topics[topic].size() > 0) {
                        for (auto subscriber : topics[topic]) {
                            int fd = subscriber.first;
                            int sf = subscriber.second;
                            string ID = fd_to_ID[fd];

                            // if subscriber is online, send it to him
                            if (find(online.begin(), online.end(), ID) !=
                                     online.end()) {
                                rc = send_all(fd, (void *) &network_length,
                                            sizeof(uint16_t));
                                if (rc < 0) {
                                    terminate_server(fds, client_count);
                                    perror("send");
                                    exit(1);
                                }

                                rc = send_all(fd, (void *) &tcp_msg,
                                        CLIENT_MESSAGE_SIZE_NO_CONTENT +
                                        content_length);
                                if (rc < 0) {
                                    terminate_server(fds, client_count);
                                    perror("send");
                                    exit(1);
                                }
                            }
                            else if (sf == 1) {
                                /* if subscriber is offline and
                                   store-and-forward is enabled, store it */
                                if (stored_messages.find(ID) == stored_messages.end()) {
                                    vector<struct tcp_to_client_message> messages;
                                    messages.push_back(tcp_msg);
                                    stored_messages[ID] = messages;
                                } else
                                    stored_messages[ID].push_back(tcp_msg);
                            }
                        }
                    }
                } else {
                    // there's a message to be read from a TCP client
                    string ID = fd_to_ID[fds[i].fd];

                    memset(buffer, 0, BUFLEN);
                    rc = recv_all(fds[i].fd, (void *) buffer,
                                    sizeof(struct tcp_to_server_message));

                    // client closed connection/error
                    if (rc <= 0) {
                        // close the connection and print corresponding message
                        close(fds[i].fd);
                        printf("Client %s disconnected.\n",
                                    fd_to_ID[fds[i].fd].c_str());

                        // remove client from online list
                        for (uint32_t j = 0; j < online.size(); j++)
                            if (online[j].compare(fd_to_ID[fds[i].fd]) == 0) {
                                online.erase(online.begin() + j);
                                break;
                            }

                        // remove client from list of connections
                        memset(&fds[client_count - 1], 0, sizeof(struct pollfd));
                        fds[i] = fds[client_count - 1];
                        client_count--;
                        continue;
                    }

                    // parse message and add/remove client from topic list
                    struct tcp_to_server_message *msg = (struct tcp_to_server_message *) buffer;

                    if (msg->msg_type == 0) {
                        string topic(msg->topic);

                        if (topics.find(topic) == topics.end()) {
                            vector<pair<int, int>> subscribers;

                            subscribers.push_back(make_pair(fds[i].fd, msg->sf));
                            topics[topic] = subscribers;
                        } else {
                            topics[topic].push_back(make_pair(fds[i].fd, msg->sf));
                        }
                    } else {
                        string topic(msg->topic);

                        // remove client from topic list
                        for (uint32_t j = 0; j < topics[topic].size(); j++)
                            if (topics[topic][j].first == fds[i].fd) {
                                topics[topic].erase(topics[topic].begin() + j);
                                break;
                            }
                    }
                }
            }
        }
    }
}

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    out.open("server_log", ofstream::out);

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
        close(listenfd);
        perror("setsockopt(SO_REUSEADDR) failed");
        exit(1);
    }

    // disable Nagle's algorithm
    rc = setsockopt(listenfd, IPPROTO_TCP, TCP_NODELAY, (char *) &enable, sizeof(int));
    if (rc < 0) {
        close(listenfd);
        perror("setsockopt(TCP_NODELAY) failed");
        exit(1);
    }

    // set address and port
    struct sockaddr_in serv_addr;
    socklen_t socket_len = sizeof(struct sockaddr_in);

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // bind the socket to the server address and port
    rc = bind(listenfd, (struct sockaddr *) &serv_addr, socket_len);
    if (rc < 0) {
        close(listenfd);
        perror("bind");
        exit(1);
    }

    // setup server UDP socket
    int udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpfd < 0) {
        close(listenfd);
        perror("socket");
        exit(1);
    }

    // make address reusable
    rc = setsockopt(udpfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    if (rc < 0) {
        close(listenfd);
        close(udpfd);
        perror("setsockopt(SO_REUSEADDR) failed");
        exit(1);
    }

    // set address and port
    struct sockaddr_in udp_addr;
    memset(&udp_addr, 0, sizeof(udp_addr));
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_port = htons(port);
    udp_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // bind the socket to the server address and port
    rc = bind(udpfd, (struct sockaddr *) &udp_addr, socket_len);
    if (rc < 0) {
        close(listenfd);
        close(udpfd);
        perror("bind");
        exit(1);
    }

    start_server(listenfd, udpfd);

    return 0;
}