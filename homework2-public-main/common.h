#ifndef __COMMON_H__
#define __COMMON_H__

#include <stddef.h>
#include <stdint.h>

int send_all(int sockfd, void *buff, size_t len);
int recv_all(int sockfd, void *buff, size_t len);

/* Dimensiunea maxima a mesajului */
#define MSG_MAXSIZE 1024

struct chat_packet {
  uint16_t len;
  char message[MSG_MAXSIZE + 1];
};

struct udp_packet {
  char topic[50];
  uint8_t type;
  char payload[1501];
};

struct topic_entry {
  char topic[50];
  int subscribers[10000];
  int nr_subscribers;
  char type[50];
  char value[1501];
};

#endif
