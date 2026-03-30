#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define BUFSIZE 1024
#define PAYLOAD_SIZE 1000

typedef struct
{
  int seq_num;
  int is_last;
  int data_size;
  char payload[PAYLOAD_SIZE];
} Packet;

typedef struct
{
  int ack_num;
} AckPacket;

void error(char *msg)
{
  perror(msg);
  exit(1);
}

void send_file(int sockfd, struct sockaddr_in *clientaddr, socklen_t clientlen, char *filename)
{
  FILE *fp = fopen(filename, "rb");
  if (!fp)
  {
    Packet p = {0, 1, 0, ""};
    sendto(sockfd, &p, sizeof(p), 0, (struct sockaddr *)clientaddr, clientlen);
    return;
  }

  Packet pkt;
  AckPacket ack;
  int seq = 0, n, retries;

  while (!feof(fp))
  {
    pkt.seq_num = seq;
    pkt.data_size = fread(pkt.payload, 1, PAYLOAD_SIZE, fp);
    pkt.is_last = (pkt.data_size < PAYLOAD_SIZE || feof(fp)) ? 1 : 0;

    retries = 0;
    while (retries < 10)
    {
      sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)clientaddr, clientlen);
      n = recvfrom(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)clientaddr, &clientlen);
      if (n > 0 && ack.ack_num == seq)
        break;
      retries++;
    }
    seq++;
    if (pkt.is_last)
      break;
  }
  fclose(fp);
}

void recv_file(int sockfd, struct sockaddr_in *clientaddr, socklen_t clientlen, char *filename)
{
  FILE *fp = fopen(filename, "wb");
  if (!fp)
    return;

  Packet pkt;
  AckPacket ack;
  int expected_seq = 0, n;

  while (1)
  {
    n = recvfrom(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)clientaddr, &clientlen);
    if (n > 0)
    {
      if (pkt.seq_num == expected_seq)
      {
        fwrite(pkt.payload, 1, pkt.data_size, fp);
        ack.ack_num = pkt.seq_num;
        sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)clientaddr, clientlen);

        expected_seq++;
        if (pkt.is_last)
          break;
      }
      else
      {
        ack.ack_num = pkt.seq_num;
        sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)clientaddr, clientlen);
      }
    }
    else
    {
      break;
    }
  }
  fclose(fp);
}

int main(int argc, char **argv)
{
  int sockfd, portno, n, optval;
  socklen_t clientlen;
  struct sockaddr_in serveraddr, clientaddr;
  char buf[BUFSIZE];

  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0)
    error("ERROR opening socket");

  optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

  struct timeval tv = {2, 0};
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  bzero((char *)&serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);

  if (bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
    error("ERROR on binding");

  clientlen = sizeof(clientaddr);
  while (1)
  {
    bzero(buf, BUFSIZE);
    n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&clientaddr, &clientlen);
    if (n <= 0)
      continue;
    char cmd[20], arg[100];
    arg[0] = '\0';
    sscanf(buf, "%s %s", cmd, arg);

    if (strcmp(cmd, "ls") == 0)
    {
      system("ls > .ls_tmp");
      send_file(sockfd, &clientaddr, clientlen, ".ls_tmp");
      remove(".ls_tmp");
    }
    else if (strcmp(cmd, "delete") == 0)
    {
      if (remove(arg) == 0)
        strcpy(buf, "File deleted");
      else
        strcpy(buf, "Error deleting file");
      sendto(sockfd, buf, strlen(buf) + 1, 0, (struct sockaddr *)&clientaddr, clientlen);
    }
    else if (strcmp(cmd, "put") == 0)
    {
      recv_file(sockfd, &clientaddr, clientlen, arg);
    }
    else if (strcmp(cmd, "get") == 0)
    {
      send_file(sockfd, &clientaddr, clientlen, arg);
    }
    else if (strcmp(cmd, "exit") == 0)
    {
      break;
    }
    else
    {
      strcpy(buf, "Command not found");
      sendto(sockfd, buf, strlen(buf) + 1, 0, (struct sockaddr *)&clientaddr, clientlen);
    }
  }
  close(sockfd);
  return 0;
}