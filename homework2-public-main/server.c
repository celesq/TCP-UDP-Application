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

#include "common.h"
#include "helpers.h"

#define MAX_CONNECTIONS SOMAXCONN
#ifndef SO_REUSEPORT
#define SO_REUSEPORT 15
#endif


void close_connection (struct pollfd *poll_fds, int i , int num_sockets) {

    close(poll_fds[i].fd);

    for (int j = i; j < num_sockets - 1; j++) {
        poll_fds[j] = poll_fds[j + 1];
    }

}

void parse_udp_message(struct udp_packet *packet, char buf[],  struct sockaddr_in *udp_client, char msg_type[], char msg_value[]) {

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(udp_client->sin_addr), ip, INET_ADDRSTRLEN);
    int port = ntohs(udp_client->sin_port);

    char value[1501], type[100];

    if (packet->type == 0) {

        strcpy(type, "INT");

        uint8_t sign = packet->payload[0];
        uint32_t number;

        memcpy(&number, packet->payload + 1, sizeof(uint32_t));

        number = ntohl(number);

        if (sign == 1) {

            sprintf(value, "-%d", number);

        } else {

            sprintf(value, "%d", number);
        }

    } else if (packet->type == 1) {

        strcpy(type, "SHORT_REAL");

        uint16_t number;

        memcpy(&number, packet->payload, sizeof(uint16_t));

        number = ntohs(number);

        sprintf(value, "%d.%02d", number / 100, number % 100);

    } else if (packet->type == 2) {

        strcpy(type, "FLOAT");

        uint8_t sign = packet->payload[0];
        uint32_t number;
        uint8_t exponent;

        memcpy(&number, packet->payload + 1, sizeof(uint32_t));
        memcpy(&exponent, packet->payload + 1 + sizeof(uint32_t), sizeof(uint8_t));

        number = ntohl(number);

        double power = 1;
        for (int i = 0; i < exponent; i++) {
            power = power * 10;
        }

        double result = number / power;

        if (sign == 1) {
            sprintf(value, "-%.*lf", exponent, result);
        } else {
            sprintf(value, "%.*lf", exponent, result);
        }


    } else if (packet->type == 3) {

        strcpy(type, "STRING");

        strcpy(value, packet->payload);

    } else {
        printf("UDP packet is wrong %d %s\n", packet->type, packet->payload);
        return;
    }

    sprintf(buf, "%s - %s - %s", packet->topic, type, value);

    strncpy(msg_type, type, sizeof(msg_type) - 1);
    strncpy(msg_value, value, sizeof(msg_value) - 1);

}

void subscribe(int id, char topic[],  struct topic_entry *topics, int *topic_count) {
    int found = -1;
    for (int i = 0; i < *topic_count; i++) {
        if (strcmp(topics[i].topic, topic) == 0) {
            found = i;
            break;
        }
    }

    if (found == -1) {
        printf("Not found \n");
        return;
    } else {
        for (int j = 0 ; j < topics[found].nr_subscribers; j++) {
            if (topics[found].subscribers[j] == id) {
                return;
            }
        }
        topics[found].subscribers[topics[found].nr_subscribers] = id;
        topics[found].nr_subscribers++;
    }
}

void unsubscribe(int id, char topic[],  struct topic_entry *topics, int *topic_count) {
    for (int i = 0; i < *topic_count; i++)  {
        if (strcmp(topics[i].topic, topic) == 0) {
            for (int j = 0; j < topics[i].nr_subscribers; j++) {
                if(topics[i].subscribers[j] == id) {
                    for (int k = j; k < topics[i].nr_subscribers - 1; k++) {
                        topics[i].subscribers[k] = topics[i].subscribers[k + 1];
                    }
                    topics[i].nr_subscribers--;
                    break;
                }
            }
            return;
        }
    }

}

void run_server(const int listen_fd_tcp, const int listen_fd_udp) {
    struct topic_entry topics[100];
    int topic_count = 0;

    int ids_connected[MAX_CONNECTIONS];
    int client_ids[MAX_CONNECTIONS];

    for (int i = 0 ; i < MAX_CONNECTIONS; i++) {
        ids_connected[i] = 0;
        client_ids[i] = 0;
    }

    struct pollfd poll_fds[MAX_CONNECTIONS];
    int num_sockets = 2;

    struct chat_packet received_packet;

    int rc = listen(listen_fd_tcp, MAX_CONNECTIONS);
    DIE(rc < 0, "Listen error\n");

    poll_fds[0].fd = listen_fd_tcp;
    poll_fds[0].events = POLLIN;

    poll_fds[1].fd = listen_fd_udp;
    poll_fds[1].events = POLLIN;

    while (1) {
        rc = poll(poll_fds, num_sockets, 0);
        DIE(rc < 0, "Poll error\n");

        for (int i = 0; i < num_sockets; i++) {
            if (poll_fds[i].revents & POLLIN) {
                if (poll_fds[i].fd == listen_fd_tcp) {
                    // cerere de conectare TCP
                    struct sockaddr_in client_addr;
                    socklen_t client_len = sizeof(struct sockaddr_in);
                    const int sock = accept(listen_fd_tcp, (struct sockaddr *)&client_addr, &client_len);
                    DIE(sock < 0, "Socket accept error \n");

                    poll_fds[num_sockets].fd = sock;
                    poll_fds[num_sockets].events = POLLIN;
                    num_sockets++;

                    char client_id[50];
                    memset(client_id, 0, sizeof(client_id));

                    rc = recv(sock, client_id, sizeof(client_id), 0);
                    DIE(rc < 0, "Error receiving client ID\n");

                    int id;
                    sscanf(client_id + 1, "%d", &id);

                    if (ids_connected[id] == 0) {
                        ids_connected[id] = 1;
                        client_ids[num_sockets - 1] = id;
                        printf("New client %s connected from %s:%d.\n",
                               client_id, inet_ntoa(client_addr.sin_addr),
                               ntohs(client_addr.sin_port));
                    } else {
                        printf("Client C%d already connected.\n", id);
                        close(sock);
                    }

                } else if (poll_fds[i].fd == listen_fd_udp) {
                    // primim mesaj de la client UDP
                    struct sockaddr_in udp_client;
                    socklen_t client_len = sizeof(struct sockaddr_in);

                    struct udp_packet udp_packet;
                    int rc = recvfrom(poll_fds[i].fd, &udp_packet, sizeof(struct udp_packet), 0,
                                      (struct sockaddr *)&udp_client, &client_len);
                    DIE(rc < 0, "Error receiving UDP\n");

                    char buf[1501], type[50], value[50];
                    parse_udp_message(&udp_packet, buf, &udp_client, type, value);

                    // trimitem mesajul UDP tuturor subscriberilor
                    int found = -1;
                    for (int t = 0; t < topic_count; t++) {
                        if (strcmp(topics[t].topic, udp_packet.topic) == 0) {
                            found = 1;
                            for (int j = 0; j < topics[t].nr_subscribers; j++) {
                                int client_id = topics[t].subscribers[j];
                                int client_sock = -1;
                                for (int k = 0; k < num_sockets; k++) {
                                    if (client_ids[k] == client_id) {
                                        client_sock = k;
                                        break;
                                    }
                                }
                                if (client_sock != -1) {
                                    rc = send(poll_fds[client_sock].fd, buf, strlen(buf) + 1, 0);
                                    DIE(rc < 0, "Error sending back to TCP clients\n");
                                }
                            }
                        }
                    }
                    if (found == -1) {
                        topics[topic_count].nr_subscribers = 0;
                        strcpy(topics[topic_count].topic, udp_packet.topic);
                        strcpy(topics[topic_count].type, type);
                        strcpy(topics[topic_count].value, value);
                        topic_count++;
                    }

                } else {
                    rc = recv_all(poll_fds[i].fd, &received_packet, sizeof(received_packet));
                    DIE(rc < 0, "Error receiving\n");

                    if (rc == 0) {
                        close_connection(poll_fds, i, num_sockets);
                        num_sockets--;
                    } else {
                        if (strncmp("exit", received_packet.message, 4) == 0) {
                            printf("Client C%d disconnected.\n", client_ids[i]);
                            close_connection(poll_fds, i, num_sockets);
                            num_sockets--;
                            ids_connected[client_ids[i]] = 0;
                            client_ids[i] = 0;
                        } else if (strncmp(received_packet.message, "subscribe", 9) == 0) {
                            subscribe(client_ids[i], received_packet.message + 10, topics, &topic_count);
                        } else if (strncmp(received_packet.message, "unsubscribe", 11) == 0) {
                            unsubscribe(client_ids[i], received_packet.message + 12, topics, &topic_count);
                        } else {
                            for (int j = 0; j < num_sockets; j++) {
                                if (j != i && poll_fds[j].fd != listen_fd_tcp) {
                                    rc = send_all(poll_fds[j].fd, &received_packet, sizeof(received_packet));
                                    DIE(rc < 0, "Error sending back\n");
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}


int main (int argc, char *argv[]) {

    if (argc != 2) {
        printf("\n Usage: ./server <port> \n");
        return 1;
    }

    setvbuf(stdout, NULL, _IONBF, 0);

    uint16_t port;
    int rc = sscanf(argv[1], "%hu", &port); //portul dat ca parametru
    DIE(rc != 1, "Given port is invalid");

    const int listen_fd_udp = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(listen_fd_udp < 0, "Socket UDP\n");

    const int listen_fd_tcp = socket(AF_INET, SOCK_STREAM, 0); //obtin socketul de ascultare
    DIE(listen_fd_tcp < 0, "Socket TCP");

    struct sockaddr_in serv_addr; //structurile necesare pt completare ip, port, familie addr
    socklen_t socket_len = sizeof(struct sockaddr_in);

    const int enable = 1; //setam SO_REUSEADDR pe socket
    if (setsockopt(listen_fd_tcp, SOL_SOCKET, SO_REUSEADDR , &enable, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    if (setsockopt(listen_fd_tcp, SOL_SOCKET,  SO_REUSEPORT, &enable, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");


    if (setsockopt(listen_fd_udp, SOL_SOCKET, SO_REUSEADDR , &enable, sizeof(int)) < 0)
        perror("setsockopt UDP");

    if (setsockopt(listen_fd_udp, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(int)) < 0)
        perror("setsockopt UDP");

    memset(&serv_addr, 0, socket_len); //initializare
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    rc = bind(listen_fd_tcp, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "Bind TCP");

    rc = bind(listen_fd_udp, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "Bind UDP\n");

    run_server(listen_fd_tcp, listen_fd_udp);

    close(listen_fd_udp);
    close(listen_fd_tcp);

    return 0;

}


