#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdint.h>

#define BUFLEN 1600
#define ID_LEN 10 // without counting '\0'
#define CONTENTLEN 1500
#define TCP_MSG_LEN 64 // longest message is 63 characters + '\0' and '\n'
#define TOPIC_SIZE 50

#define CLIENT_MESSAGE_SIZE_NO_CONTENT sizeof(struct tcp_to_client_message) - CONTENTLEN

struct udp_message {
    char topic[TOPIC_SIZE];
    uint8_t type;
    char content[CONTENTLEN];
};

struct tcp_to_client_message {
    uint32_t sender_IP;
    uint16_t sender_port;
    char topic[TOPIC_SIZE];
    uint8_t type;
    char content[CONTENTLEN];
};

struct tcp_to_server_message {
    int msg_type; // 0 = subscribe, 1 = unsubscribe
    int sf;
    char topic[TOPIC_SIZE  + 1];
};

#endif