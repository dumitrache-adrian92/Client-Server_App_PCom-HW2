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

#include "common.h"
#include "simple_io.h"

using namespace std;

ofstream out;

void close_connections(struct pollfd *fds)
{
    for (int i = 0; i < 2; i++)
        close(fds[i].fd);
}

// returns the SF from a command
int get_sf(char *buffer)
{
    for (int i = TCP_MSG_LEN - 1; i >= 0; i--)
        if (buffer[i] == '0')
            return 0;
        else if (buffer[i] == '1')
            return 1;

    return 0;
}

// returns the topic from a command
string get_topic(char *topic_start)
{
    char topic[TOPIC_SIZE + 1];

    memset(topic, 0, TOPIC_SIZE);

    while (topic_start[0] == ' ')
        topic_start++;

    for (int j = 0; j < TOPIC_SIZE; j++) {
        topic[j] = topic_start[j];

        if (topic_start[j] == ' ') {
            topic[j] = '\0';
            break;
        }

        if (topic_start[j] == '\0')
            break;
    }

    topic[50] = '\0';

    return string(topic);
}

// checks if a command is valid
bool pattern_match(char *buffer)
{
    string s(buffer);

    // if it doesn't start with "subscribe " or "unsubscribe " it's invalid
    if (strstr(buffer, "subscribe") != buffer && strstr(buffer, "unsubscribe") != buffer)
        return false;

    vector <string> tokens;
    stringstream bufStream(s);
    string aux;

    while(getline(bufStream, aux, ' '))
    {
        tokens.push_back(aux);
    }

    if (tokens[0].compare("subscribe") == 0) {
        // if it isn't in the format "subscribe <topic> <SF>", it's invalid
        if (tokens.size() != 3)
            return false;

        // if the third argument isn't 0 or 1, it's invalid
        if (tokens[2].compare("0\n") != 0 && tokens[2].compare("1\n") != 0)
            return false;
    } else if (tokens[0].compare("unsubscribe") == 0) {
        if (tokens.size() != 2)
            return false;
    }

    return true;
}

void start_client(int sockfd)
{
    int rc;
    char buffer[TCP_MSG_LEN];
    char buffer2[BUFLEN];
    struct pollfd fds[2];

    memset(buffer2, 0, BUFLEN);

    fds[0].fd = sockfd;
    fds[0].events = POLLIN;

    fds[1].fd = STDIN_FILENO;
    fds[1].events = POLLIN;

    while (1) {
        rc = poll(fds, 2, -1);
        if (rc < 0) {
            perror("poll");
            exit(1);
        }

        if (fds[1].revents & POLLIN) {
            memset(buffer, 0, TCP_MSG_LEN);
            char *ret = fgets(buffer, TCP_MSG_LEN, stdin);

            if (ret == NULL) {
                close_connections(fds);
                perror("fgets");
                exit(1);
            }

            // stop the client if the user types "exit"
            if (strncmp(buffer, "exit", 4) == 0) {
                close_connections(fds);
                close(sockfd);
                exit(0);
            }

            // check if the command is valid
            if (!pattern_match(buffer)) {
                out << "Invalid command\n";
                continue;
            }

            struct tcp_to_server_message msg;
            memset(&msg, 0, sizeof(struct tcp_to_server_message));

            if (strstr(buffer, "subscribe") == buffer) {
                msg.msg_type = 0;
                memcpy(msg.topic, get_topic(buffer + 10).c_str(), TOPIC_SIZE);
                printf("Subscribed to topic.\n");
            }
            else {
                msg.msg_type = 1;
                memcpy(msg.topic, get_topic(buffer + 12).c_str(), TOPIC_SIZE);
                printf("Unsubscribed from topic.\n");
            }

            msg.sf = get_sf(buffer);

            rc = send_all(sockfd, &msg, sizeof(struct tcp_to_server_message));
            if (rc < 0) {
                close_connections(fds);
                perror("send");
                exit(1);
            }
        }

        if (fds[0].revents & POLLIN) {
            uint16_t content_length;

            rc = recv_all(sockfd, &content_length, sizeof(uint16_t));
            if (rc < 0) {
                close_connections(fds);
                perror("recv");
                exit(1);
            }

            content_length = ntohs(content_length);

            memset(buffer2, 0, BUFLEN);
            rc = recv_all(sockfd, buffer2, CLIENT_MESSAGE_SIZE_NO_CONTENT
                                             + content_length);
            if (rc < 0) {
                close_connections(fds);
                perror("recv");
                exit(1);
            }

            if (rc == 0)
                break;

            struct tcp_to_client_message *msg = (struct tcp_to_client_message *) buffer2;
            char *aux = msg->content;
            char type[15];
            memset(type, 0, 15);
            char content[1501];
            memset(content, 0, 1501);
            char topic[51];
            memset(topic, 0, 51);

            memcpy(topic, msg->topic, 50);
            topic[50] = '\0';

            if (msg->type == 0) {
                strcpy(type, "INT");
                uint8_t sign_byte = aux[0];
                uint32_t number = ntohl(*(uint32_t *) (aux + 1));
                int to_print = number;

                if (sign_byte == 1)
                    to_print = -to_print;

                sprintf(content, "%d", to_print);
            }
            else if (msg->type == 1) {
                strcpy(type, "SHORT_REAL");

                uint16_t number = ntohs(*(uint16_t *) aux);

                uint16_t zec = number % 100;

                if (zec > 10)
                    sprintf(content, "%hu.%hu", number / 100, number % 100);
                else
                    sprintf(content, "%hu.0%hu", number / 100, number % 100);
            }
            else if (msg->type == 2) {
                strcpy(type, "FLOAT");
                uint8_t sign_byte = aux[0];
                uint32_t concatenated_number = ntohl(*(uint32_t *) (aux + 1));
                uint8_t power = aux[5];
                float div = pow(10, power);
                float result = concatenated_number / div;

                if (sign_byte == 1)
                    sprintf(content, "-%f", result);
                else
                    sprintf(content, "%f", result);
            }
            else if (msg->type == 3) {
                strcpy(type, "STRING");
                strcpy(content, aux);
            }

            printf("%s:%hu - ", inet_ntoa(*(struct in_addr *) &msg->sender_IP),
                ntohs(msg->sender_port));
            cout << topic << " - " << type << " - " << content << endl;
        }
    }
}

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    out.open("client_log", ofstream::out);

    if (argc != 4) {
        printf("Usage: %s <id_client> <ip_server> <port>\n", argv[0]);
        exit(1);
    }

    if (strlen(argv[1]) > 10) {
        printf("Client ID is too long.\n");
        exit(1);
    }

    int rc;

    // create a TCP socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(1);
    }

    // make address reusable
    int enable = 1;
    rc = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));

    // connect to server
    struct sockaddr_in server_addr;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[3]));
    rc = inet_aton(argv[2], &server_addr.sin_addr);
    if (rc < 0) {
        perror("inet_aton");
        exit(1);
    }

    rc = connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if (rc < 0) {
        perror("connect");
        exit(1);
    }

    // send id_client to server
    char buffer[ID_LEN + 1];

    memset((void *) buffer, 0, ID_LEN + 1);
    strcpy(buffer, argv[1]);
    rc = send_all(sockfd, buffer, ID_LEN + 1);
    if (rc < 0) {
        perror("send");
        exit(1);
    }

    start_client(sockfd);

    close(sockfd);

    return 0;
}