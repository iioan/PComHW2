#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/poll.h>
#include <math.h>
#include "helper.h"

#define MAX_PFDS 32

void print_message(char * buffer) {
    udp_info info;
    // extrag informatiile din buffer
    memcpy(info.ip_address, buffer, INET_ADDRSTRLEN + 1);
    memcpy(info.port, buffer + INET_ADDRSTRLEN, 6);
    memcpy(info.topic, buffer + INET_ADDRSTRLEN + 6, 50);
    memcpy( & info.type, buffer + INET_ADDRSTRLEN + 6 + 50, 1);
    memcpy(info.content, buffer + INET_ADDRSTRLEN + 6 + 50 + 1, 1500);

    printf("%s:%s - ", info.ip_address, info.port);
    printf("%s - ", info.topic);
    printf("%s - ", info.type == 0 ? "INT" : info.type == 1 ? "SHORT_REAL" :
        info.type == 2 ? "FLOAT" :
        "STRING");
    // afisez continutul mesajului in functie de tip
    if (info.type == 0) {
        uint8_t sign;
        uint32_t value;
        memcpy(&sign, info.content, 1);
        memcpy(&value, info.content + 1, 4);
        value = ntohl(value);
        if (sign == 1) {
            value = -value;
        }
        printf("%d", value);
    }

    if (info.type == 1) {
        uint16_t value;
        memcpy(&value, info.content, 2);
        value = ntohs(value);
        double num = (double) value / 100;
        printf("%.2f", num);
    }

    if (info.type == 2) {
        uint8_t power;
        uint8_t sign;
        uint32_t value;
        memcpy(&sign, info.content, 1);
        memcpy(&value, info.content + 1, 4);
        memcpy(&power, info.content + 5, 1);
        value = ntohl(value);
        double num = (double) value / pow(10, power);
        if (sign == 1)
            num = -num;
        printf("%.*f", power, num);
    }

    if (info.type == 3) {
        printf("%s", info.content);
    }
    printf("\n");
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    if (argc != 4) {
        perror("Wrong number of arguments");
        exit(EXIT_FAILURE);
    }
    // Aloc multimea de file descriptori monitorizati
    struct pollfd * pfds = calloc(MAX_PFDS, sizeof(struct pollfd));
    if (pfds == NULL) {
        perror("calloc");
        exit(EXIT_FAILURE);
    }
    int nfds = 0;

    struct sockaddr_in server;
    char * client_id = argv[1];
    char * server_ip = argv[2];
    char * server_port = argv[3];

    char * buffer = calloc(1, sizeof(udp_info));
    if (buffer == NULL) {
        perror("calloc");
        exit(EXIT_FAILURE);
    }
    char * message = calloc(1, 1501);
    if (message == NULL) {
        perror("calloc");
        exit(EXIT_FAILURE);
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // dezactivarea algoritmului lui Nagle
    int flag = 1;
    int result = setsockopt(sockfd, /* socket affected */
        IPPROTO_TCP, /* set option at TCP level */
        TCP_NODELAY, /* name of option */
        (char * ) & flag, /* the cast is historical cruft */
        sizeof(int)); /* length of option value */
    if (result < 0) {
        perror("Error: setsockopt");
        exit(EXIT_FAILURE);
    }

    // Connect
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(server_port));
    server.sin_addr.s_addr = inet_addr(server_ip);
    if (connect(sockfd, (struct sockaddr * ) & server, sizeof(server)) < 0) {
        perror("Error: connect");
        exit(EXIT_FAILURE);
    }

    // Trimit id-ul clientului
    if (send(sockfd, client_id, strlen(client_id), 0) < 0) {
        perror("Error: send");
        exit(EXIT_FAILURE);
    }

    // se citesc date de la stdin, resp de la socket-ul server-ului
    pfds[nfds].fd = STDIN_FILENO;
    pfds[nfds].events = POLLIN;
    nfds++;
    pfds[nfds].fd = sockfd;
    pfds[nfds].events = POLLIN;
    nfds++;

    int rc = 0;

    while (1) {
        memset(buffer, 0, sizeof(udp_info));
        memset(message, 0, 1501);
        rc = poll(pfds, nfds, -1);
        if (rc < 0) {
            perror("Error: poll");
            exit(EXIT_FAILURE);
        }
        if (pfds[0].revents & POLLIN) {
            // se citesc date de la stdin
            fgets(message, 1501, stdin);
            if (strncmp(message, "exit", 4) == 0) {
                printf("exit\n");
                break;
            }

            char * temp = calloc(1, 1501);
            if (temp == NULL) {
                perror("calloc");
                exit(EXIT_FAILURE);
            }
            memcpy(temp, message, 1501);
            char * token = strtok(temp, " ");
            if (token == NULL) {
                perror("Error: wrong command");
                continue;
            }

            if (strcmp(token, "subscribe") == 0) {
                token = strtok(NULL, " ");
                if (token == NULL || strcmp(token, "\n") == 0) {
                    perror("Error: wrong command");
                    continue;
                }

                token = strtok(NULL, " ");
                if (token == NULL || strcmp(token, "\n") == 0) {
                    perror("Error: wrong command");
                    continue;
                }
                int sf = atoi(token);
                if (sf != 0 && sf != 1) {
                    perror("Error: wrong command");
                    continue;
                }
                if (send(sockfd, message, strlen(message), 0) < 0) {
                    perror("Error: send");
                    continue;
                }
                printf("Subscribed to topic.\n");
            } else if (strcmp(token, "unsubscribe") == 0) {
                token = strtok(NULL, " ");
                if (token == NULL || strcmp(token, "\n") == 0) {
                    perror("Error: wrong command");
                    continue;
                }
                if (send(sockfd, message, strlen(message), 0) < 0) {
                    perror("Error: send");
                    continue;
                }
                printf("Unsubscribed from topic.\n");
            } else {
                perror("Error: wrong command");
                continue;
            }
            free(temp);
        }
        if (pfds[1].revents & POLLIN) {
            // se citesc date de la server
            memset(buffer, 0, sizeof(udp_info));
            int bytes_received = recv(sockfd, buffer, sizeof(udp_info), 0);
            if (bytes_received < 0) {
                perror("Error: recv");
                exit(EXIT_FAILURE);
            }
            if (bytes_received == 0) {
                printf("Server disconnected.\n");
                break;
            } else {
                if (strcmp(buffer, "exit") == 0) {
                    break;
                }
                print_message(buffer);
            }
        }
    }
    free(buffer);
    free(message);
    close(sockfd);
    exit(EXIT_SUCCESS);
}