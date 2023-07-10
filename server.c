#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <math.h>
#include <sys/ioctl.h>
#include "helper.h"

#define MAXLINE 1573

client_info init_client() {
    client_info client;
    client.fd = 0;
    memset(client.id, 0, 10);
    client.aloc = 10;
    client.subscribed_topics = malloc(client.aloc * sizeof(channel));
    client.no_topics = 0;
    client.unsent_topics = malloc(client.aloc * sizeof(channel));
    client.no_unsent = 0;
    return client;
}

udp_info generate_message(char * buffer, struct sockaddr_in client_address) {
    udp_info info;
    memcpy(info.topic, buffer, 50);
    char temp_buf[2];
    memcpy(temp_buf, buffer + 50, 1);
    info.type = temp_buf[0];
    memcpy(info.content, buffer + 51, 1500);
    uint16_t port = ntohs(client_address.sin_port);
    sprintf(info.port, "%hu", port);
    memcpy(info.ip_address, inet_ntoa(client_address.sin_addr), 16);
    return info;
}

int main(int argc, char * argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    if (argc != 2) {
        perror("Error: wrong number of arguments");
        exit(EXIT_FAILURE);
    }

    int tcp_socket, udp_socket, client_socket, rc;
    struct sockaddr_in tcp_address, client_address, udp_address;

    int limit = 50;
    int limit_udp = 30;
    client_info * clients = malloc(limit * sizeof(client_info));
    for (int i = 0; i < limit; i++) {
        clients[i] = init_client();
    }
    if (clients == NULL) {
        perror("Error: malloc failed");
        exit(EXIT_FAILURE);
    }
    struct pollfd *pfds = malloc(limit * sizeof(struct pollfd));
    if (pfds == NULL) {
        perror("Error: malloc failed");
        exit(EXIT_FAILURE);
    }
    udp_info *udp_msg = malloc(limit_udp * sizeof(udp_info));
    if (udp_msg == NULL) {
        perror("Error: malloc failed");
        exit(EXIT_FAILURE);
    }
    int nfds = 0;
    int nudps = 0;
    int nclients = 0;
    char *buffer = malloc(MAXLINE * sizeof(char));
    if (buffer == NULL) {
        perror("Error: malloc failed");
        exit(EXIT_FAILURE);
    }

    // Create udp socket
    udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket == -1) {
        perror("Error: socket creation failed");
        exit(EXIT_FAILURE);
    }
    int on = 1;
    if(ioctl(udp_socket, FIONBIO, (char *)&on) < 0) {
        perror("Error: ioctl");
        exit(EXIT_FAILURE);
    }

    // Create TCP socket
    tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_socket == -1) {
        perror("Error: socket creation failed");
        exit(EXIT_FAILURE);
    }
    on = 1;
    if(ioctl(udp_socket, FIONBIO, (char *)&on) < 0) {
        perror("Error: ioctl");
        exit(EXIT_FAILURE);
    }

    // permit programului sa refoloseasca portul la bind
    int optval = 1;
    if (setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("setsockopt() failed");
        exit(EXIT_FAILURE);
    }

    // deactivating Nagle's algorithm for TCP
    int flag = 1;
    int result = setsockopt(tcp_socket, /* socket affected */
        IPPROTO_TCP, /* set option at TCP level */
        TCP_NODELAY, /* name of option */
        (char *) &flag, /* the cast is historical cruft */
        sizeof(int)); /* length of option value */
    if (result < 0) {
        perror("Error: setsockopt");
        exit(EXIT_FAILURE);
    }

    // deactivate Neagle's algorithm for UDP
    result = setsockopt(udp_socket, /* socket affected */
        IPPROTO_UDP, /* set option at TCP level */
        TCP_NODELAY, /* name of option */
        (char *) &flag, /* the cast is historical cruft */
        sizeof(int)); /* length of option value */
    if (result < 0) {
        perror("Error: setsockopt");
        exit(EXIT_FAILURE);
    }

    // Bind UDP
    udp_address.sin_family = AF_INET;
    udp_address.sin_addr.s_addr = INADDR_ANY;
    udp_address.sin_port = htons(atoi(argv[1]));
    if (bind(udp_socket, (struct sockaddr * ) & udp_address, sizeof(udp_address)) == -1) {
        perror("Error: bind failed");
        exit(EXIT_FAILURE);
    }

    // Bind TCP
    memset(&tcp_address, 0, sizeof(tcp_address));
    tcp_address.sin_family = AF_INET;
    tcp_address.sin_addr.s_addr = inet_addr("127.0.0.1");
    tcp_address.sin_port = htons(atoi(argv[1]));
    if (bind(tcp_socket, (struct sockaddr * ) & tcp_address, sizeof(tcp_address)) == -1) {
        perror("Error: bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen TCP
    if (listen(tcp_socket, 10) == -1) {
        perror("Error: listen failed");
        exit(EXIT_FAILURE);
    }

    nfds = 0;
    pfds[nfds].fd = STDIN_FILENO;
    pfds[nfds].events = POLLIN;
    nfds++;
    pfds[nfds].fd = tcp_socket;
    pfds[nfds].events = POLLIN;
    nfds++;
    pfds[nfds].fd = udp_socket;
    pfds[nfds].events = POLLIN;
    nfds++;

    int exit_sock = 0;

    while (exit_sock != 1) {
        // se asteapta pana cand se primesc date pe cel putin unul din file descriptori
        rc = poll(pfds, nfds, -1);
        if (rc < 0) {
            perror("Error: poll");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < nfds; i++) {
            memset(buffer, 0, MAXLINE);
            if (pfds[i].revents & POLLIN) {
                if (pfds[i].fd == STDIN_FILENO) {
                    // citeste de la tastatura
                    fgets(buffer, MAXLINE, stdin);
                    buffer[strlen(buffer) - 1] = '\0';
                    if (strcmp(buffer, "exit") == 0) {
                        for (int j = 0; j < nclients; j++) {
                            if (clients[j].fd > 0) {
                                send(clients[j].fd, buffer, strlen(buffer), 0);
                            }
                        }
                        exit_sock = 1;
                        break;
                    }
                } else if (pfds[i].fd == tcp_socket) {
                    // a venit o cerere de conexiune pe socketul inactiv(cel cu listen), pe care serverul o accepta
                    int client_addrlen = sizeof(client_address);

                    client_socket = accept(tcp_socket, (struct sockaddr *) &client_address, (socklen_t *) &client_addrlen);
                    if (client_socket == -1) {
                        perror("Error: accept failed");
                        exit(EXIT_FAILURE);
                    }
                    int get_client_id = read(client_socket, buffer, 200);
                    if (get_client_id == -1) {
                        perror("Error: read failed");
                        exit(EXIT_FAILURE);
                    }

                    int backup = 0;
                    int duplicate = 0;
                    int reconnected = 0;
                    for (int j = 0; j < nclients; j++) {
                        if (strcmp(clients[j].id, buffer) == 0) {
                            if (clients[j].fd == 0) {
                                backup = j;
                                clients[j].fd = client_socket;
                                reconnected = 1;
                                break;
                            } else {
                                duplicate = 1;
                                break;
                            }
                        }
                    }
                    if (duplicate == 1) {
                        printf("Client %s already connected.\n", buffer);
                        char exit_message[5] = "exit";
                        int send_exit_message = send(client_socket, exit_message, strlen(exit_message), 0);
                        if (send_exit_message == -1) {
                            perror("Error: send failed");
                            exit(EXIT_FAILURE);
                        }
                        close(client_socket);
                        continue;
                    } else {
                        printf("New client %s connected from %s:%i.\n", buffer, 
                                    inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
                        if (reconnected == 1) {
                            pfds[nfds].fd = client_socket;
                            pfds[nfds].events = POLLIN;
                            int m = clients[backup].no_unsent - 1;
                            while (clients[backup].no_unsent > 0) {
                                if (clients[backup].unsent_topics[m].sf == 1) {
                                    int send_message = send(pfds[nfds].fd, (char * ) & clients[backup].unsent_topics[m].msg, sizeof(udp_info), 0);
                                    if (send_message == -1) {
                                        perror("Error: send failed");
                                        exit(EXIT_FAILURE);
                                    }
                                }
                                // golim mesajele trimise
                                memset( &clients[backup].unsent_topics[m].msg, 0, sizeof(udp_info));
                                clients[backup].unsent_topics[m].sf = 0;
                                m--;
                                clients[backup].no_unsent--;
                            }
                            nfds++;
                            break;
                        } else {
                            if (nclients == limit - 1) {
                                limit *= 2;
                                clients = realloc(clients, limit * sizeof(client_info));
                                // initializez cealalta jumatate de clienti
                                for (int j = limit / 2; j < limit; j++) {
                                    clients[j] = init_client();
                                }
                                pfds = realloc(pfds, limit * sizeof(struct pollfd));
                            }
                            clients[nclients].fd = client_socket;
                            strcpy(clients[nclients].id, buffer);
                            nclients++;
                            pfds[nfds].fd = client_socket;
                            pfds[nfds].events = POLLIN;
                            nfds++;
                            break;
                        }
                    }
                } else if (pfds[i].fd == udp_socket) {
                    memset(buffer, 0, MAXLINE);
                    int client_addrlen = sizeof(client_address);

                    client_socket = recvfrom(udp_socket, buffer, MAXLINE, 0, 
                                (struct sockaddr *) &client_address, (socklen_t *) &client_addrlen);
                    if (client_socket == -1) {
                        perror("Error: accept failed");
                        exit(EXIT_FAILURE);
                    }

                    udp_info info;
                    info = generate_message(buffer, client_address);

                    int duplicate = 0;
                    for (int k = 0; k < nudps; k++) {
                        if (strcmp(udp_msg[k].topic, info.topic) == 0) {
                            duplicate = 1;
                        }
                    }
                    if (duplicate == 0) {
                        if (nudps == limit_udp - 1) {
                            limit_udp *= 2;
                            udp_msg = realloc(udp_msg, limit_udp * sizeof(udp_info));
                        }
                        memcpy(&udp_msg[nudps], &info, sizeof(info));
                        nudps++;
                    }
                    for (int j = 0; j < nclients; j++) {
                        if (clients[j].no_topics > 0) {
                            for (int k = 0; k < clients[j].no_topics; k++) {
                                if (strcmp(clients[j].subscribed_topics[k].msg.topic, info.topic) == 0) {
                                    if (clients[j].fd != 0) {
                                        if (send(clients[j].fd, (char *)&info, MAXLINE, 0) < 0) {
                                            perror("Error: send failed");
                                            exit(EXIT_FAILURE);
                                        }
                                        break;
                                    } else if (clients[j].subscribed_topics[k].sf == 1) {
                                        int m = clients[j].no_unsent;
                                        if (m == clients[j].aloc - 1) {
                                            clients[j].aloc *= 2;
                                            clients[j].unsent_topics = realloc(clients[j].unsent_topics, clients[j].aloc * sizeof(udp_info));
                                            clients[j].subscribed_topics = realloc(clients[j].subscribed_topics, clients[j].aloc * sizeof(udp_info));
                                        }
                                        memcpy( & clients[j].unsent_topics[m].msg, & info, sizeof(info));
                                        clients[j].unsent_topics[m].sf = 1;
                                        clients[j].no_unsent++;
                                    }
                                }
                            }
                        }
                    }
                } else {
                    // am primit date pe unul din socketii cu care vorbesc cu clientii
                    memset(buffer, 0, MAXLINE);
                    int bytes_received = recv(pfds[i].fd, buffer, 1500, 0);
                    buffer[strlen(buffer) - 1] = '\0';
                    int l;
                    for (l = 0; l < nclients; l++) {
                        if (clients[l].fd == pfds[i].fd) {
                            break;
                        }
                    }
                    if (bytes_received < 0) {
                        perror("Error: recv failed");
                        exit(EXIT_FAILURE);
                    }
                    if (bytes_received == 0) {
                        // conexiunea s-a inchis
                        printf("Client %s disconnected.\n", clients[l].id);
                        close(pfds[i].fd);
                        pfds[i].fd = -1;
                        clients[l].fd = 0;
                        nfds--;
                    } else {
                        char * token = strtok(buffer, " ");
                        if (token == NULL) {
                            perror("Error: Empty command");
                            continue;
                        }
                        if (strcmp(token, "subscribe") == 0) {
                            token = strtok(NULL, " ");
                            char topic[50];
                            memcpy(topic, token, 50);
                            token = strtok(NULL, " ");
                            int sf = atoi(token);
                            int j;
                            for (j = 0; j < nudps; j++) {
                                if (strcmp(udp_msg[j].topic, topic) == 0) {
                                    int k = clients[l].no_topics;
                                    int duplicate = 0;
                                    for (int l = 0; l < k; l++) {
                                        if (strcmp(clients[l].subscribed_topics[l].msg.topic, topic) == 0) {
                                            duplicate = 1;
                                            break;
                                        }
                                    }
                                    if (duplicate == 0) {
                                        if (k == clients[l].aloc - 1) {
                                            clients[l].aloc *= 2;
                                            clients[l].subscribed_topics = realloc(clients[l].subscribed_topics, clients[l].aloc * sizeof(udp_info));
                                            clients[l].unsent_topics = realloc(clients[l].unsent_topics, clients[l].aloc * sizeof(udp_info));
                                        }
                                        memcpy( & clients[l].subscribed_topics[k].msg, & udp_msg[j], MAXLINE);
                                        clients[l].subscribed_topics[k].sf = sf;
                                        clients[l].no_topics++;
                                        break;
                                    }
                                }
                            }
                        }
                        if (strcmp(token, "unsubscribe") == 0) {
                            token = strtok(NULL, " ");
                            char topic[50];
                            memcpy(topic, token, 50);
                            int k = clients[l].no_topics;
                            for (int j = 0; j < k; j++) {
                                if (strcmp(clients[l].subscribed_topics[j].msg.topic, topic) == 0) {
                                    for (int m = j; m < k - 1; m++) {
                                        memcpy( & clients[l].subscribed_topics[m].msg, & clients[l].subscribed_topics[m + 1].msg, MAXLINE);
                                        clients[l].subscribed_topics[m].sf = clients[l].subscribed_topics[m + 1].sf;
                                    }
                                    clients[l].no_topics--;
                                    break;
                                }
                            }
                        }
                        if (strcmp(buffer, "exit") == 0) {
                            printf("Client %s disconnected.\n", clients[l].id);
                            close(pfds[i].fd);
                            pfds[i].fd = -1;
                            clients[l].fd = 0;
                            nfds--;
                        }
                    }        
                }
            }
        }
    }
    for (int i = 0; i < nfds; i++) {
        close(pfds[i].fd);
    }
    free(pfds);
    for (int i = 0; i < limit; i++) {
        free(clients[i].subscribed_topics);
        free(clients[i].unsent_topics);
    }
    free(clients);
    free(udp_msg);
    close(client_socket);
    close(tcp_socket);
    close(udp_socket);
    exit(EXIT_SUCCESS);
}