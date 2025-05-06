#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "helpers.h"

void run_client(int sock_fd, char *argv[]) {

    char buf[MSG_MAXSIZE];
    memset(buf, 0, MSG_MAXSIZE + 1);

    struct chat_packet sent_packet;
    struct chat_packet recv_packet;

    struct pollfd poll_fds[2];

    poll_fds[0].fd = sock_fd; //primim msj de la server
    poll_fds[0].events = POLLIN;

    poll_fds[1].fd = STDIN_FILENO; //citim de la tastatura
    poll_fds[1].events = POLLIN;

    while(1) {

        int rc = poll(poll_fds, 2, 0);
        DIE(rc < 0, "Error poll\n");

        if(poll_fds[0].revents & POLLIN) {

            //primim mesaj de la server

            char input_buf[1600];
            memset(input_buf, 0, sizeof(input_buf));

            int rc = recv(poll_fds[0].fd, input_buf, sizeof(input_buf) - 1, 0);

            if (rc < 0) {
                printf("Error recieving message\n");
                break;
            } else if (rc == 0) {
                printf("Closing client\n");
                break;
            }

            printf("%s\n" , input_buf);

        }

        if (poll_fds[1].revents == POLLIN) {

            //primim de la tastatura
            fgets(buf, sizeof(buf), stdin);
            buf[strcspn(buf, "\n")] = '\0';

            sent_packet.len = sizeof(buf) + 1;
            strcpy(sent_packet.message, buf);

            rc = send_all(sock_fd, &sent_packet, sizeof(sent_packet));

            if (strncmp(buf, "subscribe", 9) == 0) {
                printf("Subscribed to topic %s\n", buf + 10);

            } else if (strncmp(buf, "unsubscribe", 11) == 0) {
                printf("Unsubscribed from topic %s\n", buf + 12);
            } else if (strncmp(buf, "exit", 4) == 0) {
                printf("Disconnected from server\n");
                break;
            }

            if (rc < 0) {
                printf("Error sending back to server\n");
                break;
            }
        }

    }


}

int main (int argc, char *argv[]) {

    if (argc != 4) {
        printf("Usage ./subscriber <ID_CLIENT> <IP_SERVER> <PORT_SERVER> \n");
        return 1;
    }

    setvbuf(stdout, NULL, _IONBF, 0);

    uint16_t port;
    int rc = sscanf(argv[3], "%hu", &port);
    DIE(rc < 0, "Invalid port\n");

    const int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sock_fd < 0, "Error creating socket \n");

    struct sockaddr_in server_addr;
    socklen_t server_len = sizeof(struct sockaddr_in);

    memset(&server_addr, 0, server_len);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    rc = inet_pton(AF_INET, argv[2], &server_addr.sin_addr.s_addr);
    DIE(rc <= 0, "Error inet_pton\n");

    rc = connect(sock_fd, (struct sockaddr *)&server_addr, server_len);

    //printf("Client connected\n");

    DIE(rc < 0, "Error connecting\n");

    char id[50];
    strncpy(id, argv[1], 49);

    rc = send(sock_fd, id, 50, 0);
    DIE(rc < 0, "Error sending client id\n");

    run_client(sock_fd, argv);

    close(sock_fd);

    return 0;

}