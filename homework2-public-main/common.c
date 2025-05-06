#include "common.h"

#include <sys/socket.h>
#include <sys/types.h>

/*
    TODO 1.1: Rescrieți funcția de mai jos astfel încât ea să facă primirea
    a exact len octeți din buffer.
*/
int recv_all(int sockfd, void *buffer, size_t len) {
  size_t bytes_received = 0;
  size_t bytes_remaining = len;
    while(bytes_remaining) {
      int recieved = recv(sockfd, buffer, len, 0);
      bytes_received = bytes_received + recieved;
      bytes_remaining = bytes_remaining - recieved;
    }


  /*
    TODO: Returnam exact cati octeti am citit
  */
  return bytes_received;
}

/*
    TODO 1.2: Rescrieți funcția de mai jos astfel încât ea să facă trimiterea
    a exact len octeți din buffer.
*/

int send_all(int sockfd, void *buffer, size_t len) {
  size_t bytes_sent = 0;
  size_t bytes_remaining = len;
    while(bytes_remaining) {
      int sent = send(sockfd, buffer, bytes_remaining, 0);
      bytes_remaining = bytes_remaining - sent;
      bytes_sent = bytes_sent + sent;
    }

  /*
    TODO: Returnam exact cati octeti am trimis
  */
  return bytes_sent;
}
