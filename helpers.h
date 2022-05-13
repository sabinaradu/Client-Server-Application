#ifndef _HELPERS_H
#define _HELPERS_H 1

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define BUFLEN_SERV sizeof(udp_msg)
#define BUFLEN_CLI sizeof(fwd_msg)
#define COMMAND_LEN 65
#define DELIM " \n"
#define EXIT_SERVER "exit\n"
#define EXIT_SIGNAL "exit"
#define MAX_CLIENTS INT_MAX

struct udp_msg {
    char topic[50];
    uint8_t data_type;
    char content[1501];
} __attribute__((packed));

struct fwd_msg {
    char ip[16];
    uint16_t port;
    char topic[51];
    char data_type[11];
    char content[1501];
} __attribute__((packed));

#endif