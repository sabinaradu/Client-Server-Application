#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <math.h>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include "helpers.h"

using namespace std;

void usage(char *file) {
    fprintf(stderr, "Usage: %s server_port\n", file);
    exit(0);
}

/**
 * @param client_id             ID-ul clientului la care trebuiesc trimise
 *                                  mesajele stocate pentru abonamentele cu SF
 * @param client_messages       contine mesajele pentru abonamentele cu SF, 
 *                                  primite cat clientul este deconectat, 
 *                                  in ordinea in care sunt primite
 * @param client_subscriptions  contine abonamentele clientilor
 * @param clients_sock          contine socket-urile atribuite clientilor
 */

void send_sf_messages(
    char *client_id,
    unordered_map<string, unordered_map<string, int>> client_subscriptions,
    unordered_map<string, vector<fwd_msg>> *client_messages,
    unordered_map<string, int> clients_sock) {

    string id(client_id);
    int n;

    //se verifica daca clientul este abonat la topicul respectiv
    //si daca sunt mesaje care asteapta sa fie trimise
    if (client_subscriptions.find(id) != client_subscriptions.end() &&
        client_messages->find(id) != client_messages->end()) {
        // se trimit catre client toate mesajele stocate apoi sunt sterse
        if (!client_messages->at(client_id).empty()) {
            for (auto &msg : client_messages->at(id)) {
                n = send(clients_sock[id], (char *)&msg, sizeof(msg), 0);
                if(n == -1) 
                    return;
            }
            client_messages->at(id).clear();
        }
    }
}


int main(int argc, char *argv[]) {

    if (argc < 2) usage(argv[0]);
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    unordered_map<string, unordered_map<string, int>> client_subscriptions;
    unordered_map<string, vector<fwd_msg>> client_messages;
    unordered_map<string, bool> client_online; //starea clientului
    unordered_map<string, int> client_sock;
    socklen_t clilen;
    struct sockaddr_in serv_addr_tcp, serv_addr_udp, cli_addr;
    char buffer[BUFLEN_SERV];
    char *token;
    bool forever = true;
    int n, i, ret, flag = 1;
    int tcp_sock;
    int udp_sock;
    int newsockfd;
    int portno;

    fd_set read_fds;  // folosita in select()
    fd_set tmp_fds;   // temporar

    // se goleste multimea de descriptori de citire (read_fds) si multimea
    // temporara (tmp_fds)

    tcp_sock = socket(AF_INET, SOCK_STREAM, 0);

    if(tcp_sock < 0) return 0;

    FD_ZERO(&read_fds); //se golesc multimile
    FD_ZERO(&tmp_fds);

    udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(udp_sock < 0) 
        return 0;

    memset((char *)&serv_addr_udp, 0, sizeof(serv_addr_udp));
    serv_addr_udp.sin_family = AF_INET;
    serv_addr_udp.sin_addr.s_addr = INADDR_ANY;
    serv_addr_udp.sin_port = htons(atoi(argv[1]));

    memset((char *)&serv_addr_tcp, 0, sizeof(serv_addr_tcp));
    serv_addr_tcp.sin_family = AF_INET;
    serv_addr_tcp.sin_addr.s_addr = INADDR_ANY;
    serv_addr_tcp.sin_port = htons(atoi(argv[1]));


    ret = bind(tcp_sock, (struct sockaddr *)&serv_addr_tcp,
               sizeof(struct sockaddr));
    if(ret < 0) 
        return 0;

    ret = bind(udp_sock, (struct sockaddr *)&serv_addr_udp,
               sizeof(struct sockaddr));
    if(ret < 0) 
        return 0;

    ret = listen(tcp_sock, MAX_CLIENTS);
    if(ret < 0) 
        return 0;

    // se adauga un socket pe care se primesc conexiuni in
    // multimea read_fds
    FD_SET(tcp_sock, &read_fds);
    FD_SET(udp_sock, &read_fds);
    // se adauga stdin in multime
    FD_SET(STDIN_FILENO, &read_fds);
    //valoare maxima fd din multimea read_fds
    int fdmax = tcp_sock > udp_sock ? tcp_sock : udp_sock;


    
    while (forever) {
        tmp_fds = read_fds;

        ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
        if (ret < 0) 
            return 0;

        for (i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &tmp_fds) && i == tcp_sock) {
                // a aparut o cerere de conexiune pe socketul inactiv si
                // serverul o accepta
                clilen = sizeof(cli_addr);
                newsockfd =
                    accept(tcp_sock, (struct sockaddr *)&cli_addr, &clilen);
                if (newsockfd < 0) return 0;

                // pentru dezactivarea algoritmul lui Nagle
                ret = setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY,
                                    (char *)&flag, sizeof(int));
                if (ret < 0) return 0;

                // adaugarea socketului returnat de accept() la multimea de
                // citire
                FD_SET(newsockfd, &read_fds);

                fdmax = newsockfd > fdmax ? newsockfd : fdmax;

                // se primeste ID-ul clientului TCP
                memset(buffer, 0, BUFLEN_SERV);
                n = recv(newsockfd, buffer, BUFLEN_SERV, 0);
                if (n < 0) return 0;

                unordered_map<string, bool>::iterator it =
                    client_online.find(string(buffer));
                // verific daca clientul este conectat
                if (it == client_online.end() || it->second == false) {

                    printf("New client %s connected from %s:%d.\n", buffer,
                            inet_ntoa(cli_addr.sin_addr), 
                            ntohs(cli_addr.sin_port));

                    client_sock[string(buffer)] = newsockfd;
                    client_online[string(buffer)] = true;

                    // dupa reconectare se trimit clientului mesajele pentru
                    // topicurile la care este abonat cu SF
                    send_sf_messages(buffer, client_subscriptions,
                                        &client_messages, client_sock);
                } else {
                    printf("Client %s already connected.\n", buffer);

                    n = send(newsockfd, EXIT_SIGNAL,
                                    strlen(EXIT_SIGNAL) + 1, 0);
                    if(n == -1) 
                        return 0;

                    // inchid socket-ul si il scot din multimea de citire
                    close(newsockfd);
                    FD_CLR(newsockfd, &read_fds);
                }

            } else if (FD_ISSET(i, &tmp_fds) && i == STDIN_FILENO) {
                // daca serverul primeste comanda "exit", se inchid
                // conexiunile cu toti clientii se inchide serverul

                memset(buffer, 0, BUFLEN_SERV);
                fgets(buffer, BUFLEN_SERV - 1, stdin);

                if (strcmp(buffer, EXIT_SERVER) != 0) {
                    // se iese din bucla "while" si urmeaza sa se inchida
                    // conexiunea cu toti clientii TCP
                    fprintf(stderr, 
                    "The only accepted command is \"exit\".\n");
                    break;
                }

                forever = false;

            } else if (FD_ISSET(i, &tmp_fds) && i == udp_sock) {
                // s-au primit date pe unul din socketii de client udp si
                // serverul le receptioneaza
                fwd_msg forward_message;
                udp_msg *udp_message = (udp_msg *)buffer;

                memset(buffer, 0, BUFLEN_SERV);
                n = recv(i, buffer, BUFLEN_SERV, 0);
                if (n < 0) return 0;


                strcpy(forward_message.ip,
                        inet_ntoa(serv_addr_udp.sin_addr));
                strcpy(forward_message.topic, udp_message->topic);
                forward_message.port = ntohs(serv_addr_udp.sin_port);

                bool forward = true;
                switch (udp_message->data_type) {
                    case 0: {
                        long long result = ntohl(*(uint32_t *)
                        (udp_message->content + 1));

                        if (udp_message->content[0]) result = -1 * result;

                        strcpy(forward_message.data_type, "INT");
                        sprintf(forward_message.content, "%lld", result);
                    } break;

                    case 1: {
                        double result = ntohs(*(uint16_t *)
                        (udp_message->content));
                        result /= 100;

                        strcpy(forward_message.data_type, "SHORT_REAL");
                        sprintf(forward_message.content, "%.2lf", result);
                    } break;

                    case 2: {
                        float result = ntohl(*(uint32_t *)
                        (udp_message->content + 1));
                        result = result / pow(10, udp_message->content[5]);

                        if (udp_message->content[0]) result = -1 * result;

                        strcpy(forward_message.data_type, "FLOAT");
                        sprintf(forward_message.content, "%f", result);
                    } break;

                    case 3:
                        strcpy(forward_message.data_type, "STRING");
                        strcpy(forward_message.content, udp_message->content);
                        break;

                    default:
                        fprintf(stderr, "Invalid data type!\n");
                        forward = false;
                        break;
                }

                if (forward) {
                    string topic(udp_message->topic);
                    for (auto &subs : client_subscriptions) {
                        if (subs.second.find(topic) == subs.second.end())
                            continue;
                        // daca clientul este online, se trimite mesajul
                        if (!client_online[subs.first]) {
                            // clientul nu este online, se verifica daca este
                            // abonat cu SF 1
                            if (subs.second[topic] == 1) {
                                // este abonat cu SF 1, mesajul se pastreaza
                                if (client_messages.find(subs.first) != 
                                client_messages.end())
                                    client_messages.at(subs.first).emplace_back
                                    (forward_message);
                                else {
                                    vector<fwd_msg> new_msg;
                                    new_msg.emplace_back(forward_message);
                                    client_messages.insert(
                                        {subs.first, new_msg});
                                }
                            }
                            
                        } else {
                            int n = send(client_sock[subs.first], 
                                        (char *)&forward_message,
                                        sizeof(forward_message), 0);
                            if(n == -1) return 0;
                        }
                        
                    }
                }

            } else if (FD_ISSET(i, &tmp_fds)) {
                // s-au primit date pe unul din socketii de client tcp si 
                //serverul le receptioneaza

                memset(buffer, 0, BUFLEN_SERV);
                n = recv(i, buffer, sizeof(buffer), 0);
                if (n < 0) return 0;

                // se identifica id-ul clientului care trimite date
                string client_id;
                for (auto &elem : client_sock) {
                    if (elem.second != i) continue;

                    client_id = elem.first;
                    break;
                }

                if (n == 0) {
                    // se opreste conexiunea
                    printf("Client %s disconnected.\n", client_id.c_str());

                    // se seteaza clientul ca fiind deconectat
                    client_sock.erase(client_id);
                    client_online[client_id] = false;

                    // se inchide socket-ul si este scos din multimea de
                    // citire
                    close(i);
                    FD_CLR(i, &read_fds);
                    break;
                }
                
                token = strtok(buffer, DELIM);

                //clientul TCP verifica inainte de a trimite comanda
                if (strcmp(token, "unsubscribe") == 0) {
                    token = strtok(NULL, DELIM);

                    if (client_subscriptions[client_id].find(token) !=
                        client_subscriptions[client_id].end()) {

                        client_subscriptions[client_id].erase(token);
                        break;
                    }
                    fprintf(stderr,"Client is not subscribed to this "
                            "topic!\n");
                } else if (strcmp(token, "subscribe") == 0) {
                    token = strtok(NULL, DELIM);

                    // adaug un abonament nou pentru client
                    client_subscriptions[client_id][token] = 
                    atoi(strtok(NULL, DELIM));
                }
            }  
        }
    }
    int j = 0;
    while (j <= fdmax) {
        if (FD_ISSET(j, &read_fds) && j != STDIN_FILENO && j != tcp_sock &&
            j != udp_sock) {
            int n;
            n = send(j, EXIT_SIGNAL, strlen(EXIT_SIGNAL) + 1, 0);
            if(n == -1) return 0;
            close(j);
        }
        j++;
    }
    close(tcp_sock);
    close(udp_sock);

    return 0;
}
