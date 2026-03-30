/* * udp_client.c - Reliable FTP Client
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
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
    exit(0);
}

void send_file(int sockfd, struct sockaddr_in *serveraddr, socklen_t serverlen, char *filename)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp)
    {
        printf("Error: Could not open local file %s\n", filename);
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
        while (retries < 5)
        {
            sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)serveraddr, serverlen);
            n = recvfrom(sockfd, &ack, sizeof(ack), 0, NULL, NULL);

            if (n > 0 && ack.ack_num == seq)
                break;
            retries++;
        }
        if (retries == 5)
        {
            fprintf(stderr, "Error: Server timeout during upload.\n");
            break;
        }
        seq++;
        if (pkt.is_last)
            break;
    }
    fclose(fp);
}

void recv_file(int sockfd, struct sockaddr_in *serveraddr, socklen_t serverlen, char *filename, int print_to_stdout)
{
    FILE *fp = print_to_stdout ? stdout : fopen(filename, "wb");
    if (!fp)
    {
        printf("Error: Could not create local file %s\n", filename);
        return;
    }

    Packet pkt;
    AckPacket ack;
    int expected_seq = 0, n;

    while (1)
    {
        n = recvfrom(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)serveraddr, &serverlen);
        if (n > 0)
        {
            if (pkt.seq_num == expected_seq)
            {
                fwrite(pkt.payload, 1, pkt.data_size, fp);
                ack.ack_num = pkt.seq_num;
                sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)serveraddr, serverlen);

                expected_seq++;
                if (pkt.is_last)
                    break;
            }
            else
            {
                ack.ack_num = pkt.seq_num;
                sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)serveraddr, serverlen);
            }
        }
        else
        {
            fprintf(stderr, "Error: Server timeout during download.\n");
            break;
        }
    }
    if (!print_to_stdout)
        fclose(fp);
}

int main(int argc, char **argv)
{
    int sockfd, portno, n;
    socklen_t serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char buf[BUFSIZE];

    if (argc != 3)
    {
        fprintf(stderr, "usage: %s <hostname/IP> <port>\n", argv[0]);
        exit(0);
    }
    server = gethostbyname(argv[1]);
    portno = atoi(argv[2]);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    struct timeval tv = {2, 0};
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (server == NULL)
    {
        fprintf(stderr, "ERROR, no such host as %s\n", argv[1]);
        exit(0);
    }

    bzero((char *)&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr_list[0], (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);
    serverlen = sizeof(serveraddr);

    while (1)
    {
        printf("ftp> ");
        bzero(buf, BUFSIZE);
        fgets(buf, BUFSIZE, stdin);
        buf[strcspn(buf, "\n")] = 0;
        if (strlen(buf) == 0)
            continue;

        n = sendto(sockfd, buf, strlen(buf) + 1, 0, (struct sockaddr *)&serveraddr, serverlen);
        if (n < 0)
            error("ERROR in sendto");

        char cmd[20], arg[100];
        arg[0] = '\0';
        sscanf(buf, "%s %s", cmd, arg);

        if (strcmp(cmd, "exit") == 0)
        {
            break;
        }
        else if (strcmp(cmd, "ls") == 0)
        {
            recv_file(sockfd, &serveraddr, serverlen, NULL, 1);
        }
        else if (strcmp(cmd, "delete") == 0)
        {
            n = recvfrom(sockfd, buf, BUFSIZE, 0, NULL, NULL);
            if (n > 0)
                printf("%s\n", buf);
            else
                fprintf(stderr, "Error: Server timeout.\n");
        }
        else if (strcmp(cmd, "put") == 0)
        {
            send_file(sockfd, &serveraddr, serverlen, arg);
        }
        else if (strcmp(cmd, "get") == 0)
        {
            recv_file(sockfd, &serveraddr, serverlen, arg, 0);
        }
        else
        {
            n = recvfrom(sockfd, buf, BUFSIZE, 0, NULL, NULL);
            if (n > 0)
                printf("%s\n", buf);
            else
                fprintf(stderr, "Error: Server timeout.\n");
        }
    }
    close(sockfd);
    return 0;
}