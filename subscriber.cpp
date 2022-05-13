#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "helpers.h"

void usage(char *file) {
    fprintf(stderr, "Usage: %s id_client server_address server_port\n", file);
    exit(0);
}

int main(int argc, char *argv[]) {

    if (argc < 4) {
        usage(argv[0]);
    }

    if (strlen(argv[1]) > 10) {
        fprintf(stderr, "Client ID should have maximum 10 characters\n");
        exit(0);
    }

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    fwd_msg *received;
    struct sockaddr_in serv_addr;
    char buffer[BUFLEN_CLI];
    char *buffer_copy;
    char *token;
    char *arg1, *arg2;
    char *arg3, *arg4;
    bool subscribe;
    bool invalid;
    int sockfd, n, ret, flag = 1;
    
    fd_set read_fds;
    fd_set tmp_fds;

    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) return 0;

    serv_addr.sin_port = htons(atoi(argv[3]));
    serv_addr.sin_family = AF_INET;
    
    ret = inet_aton(argv[2], &serv_addr.sin_addr);
    if(ret == 0) return 0;

    ret = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if(ret < 0) return 0;

    FD_SET(STDIN_FILENO, &read_fds);
    FD_SET(sockfd, &read_fds);    
    
    // dezactivez algoritmul lui Nagle
    ret = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag,
                     sizeof(int));
    if(ret < 0) 
        return 0;

    // se trimite ID-ul clientului TCP
    n = send(sockfd, argv[1], strlen(argv[1]) + 1, 0);
    if(n < 0) 
        return 0;

    while (1) {
        tmp_fds = read_fds;
        ret = select(sockfd + 1, &tmp_fds, NULL, NULL, NULL);
        if(ret < 0)
            return 0;

        if (FD_ISSET(sockfd, &tmp_fds)) {
            // se primeste mesaj de la server
            memset(buffer, 0, BUFLEN_CLI);
            n = recv(sockfd, buffer, BUFLEN_CLI, 0);
            if(n < 0) return 0;

            if (strcmp(buffer, EXIT_SIGNAL) == 0) break;

            received = (fwd_msg *)buffer;
            printf("%s:%d - %s - %s - %s\n", received->ip, received->port,
                   received->topic, received->data_type, received->content);
        }

        if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
            memset(buffer, 0, BUFLEN_CLI);
            fgets(buffer, COMMAND_LEN - 1, stdin);
            
            subscribe = true;
            invalid = false;

            buffer_copy = strdup(buffer);
            token = strtok(buffer, DELIM);


            if (strcmp(buffer, EXIT_SIGNAL) == 0) {
                break;
            } else if (strcmp(token, "unsubscribe") == 0) {
                subscribe = false;
                token = strtok(NULL, DELIM);
                arg1 = token;

                token = strtok(NULL, DELIM);
                arg2 = token;

                if (!arg1) {
                    fprintf(stderr, "Topic is missing!\n");
                    invalid = true;
                }

                if (arg2) {
                    fprintf(stderr,
                            "Too many arguments in unsubscribe command!\n");
                    invalid = true;
                }

            } else if (strcmp(token, "subscribe") == 0) {
                token = strtok(NULL, DELIM);
                arg1 = token;

                token = strtok(NULL, DELIM);
                arg2 = token;

                token = strtok(NULL, DELIM);
                arg3 = token;

                token = strtok(NULL, DELIM);
                arg4 = token;

                if (!arg1) {
                    fprintf(stderr, "Topic is missing!\n");
                    invalid = true;
                }

                if (!arg2) {
                    fprintf(stderr, "SF is missing!\n");
                    invalid = true;
                }

                int SF = atoi(arg2);
                if (SF != 0 && SF != 1) {
                    fprintf(stderr, "Invalid SF value!\n");
                    invalid = true;
                }

                if (arg3) {
                    fprintf(stderr,
                            "Too many arguments in subscribe command!\n");
                    invalid = true;
                }
            } else {
                fprintf(stderr, "Invalid command!\n");
                invalid = true;
            }

            if (!invalid) {
                // se trimite mesaj la server
                n = send(sockfd, buffer_copy, strlen(buffer_copy), 0);
                if(n < 0) return 0;

                if (subscribe) {
                    printf("Subscribed to topic.\n");
                } else {
                    printf("Unsubscribed from topic.\n");
                }
            }

            free(buffer_copy);
        }   
    }

    close(sockfd);
    return 0;
}
