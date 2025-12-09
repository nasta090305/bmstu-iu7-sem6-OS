#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include "common.h"
#include <sched.h>

#define FILENAME "cln.txt"

void request(int sockfd)
{
    int index;
    char buf[BUF_SIZE];
    buf[0] = 'r';
    if (send(sockfd, &buf, sizeof(buf), 0) == -1)
    {
        perror("send");
        exit(1);
    }

    if (recv(sockfd, &buf, sizeof(buf), 0) == -1)
    {
        perror("recieve");
        exit(1);
    }

    printf("Recieved: ");
    for (size_t i = 0; i < ARR_SIZE; i++)
        printf("%c ", buf[i]);
    puts("");

    index = -1;
    for (int i = 0; i < ARR_SIZE; i++)
    {
        if (buf[i] != ' ')
        {
            index = i;
            break;
        }
    }

    if (index == -1)
    {
        printf("No availible letters left\n");
        exit(1);
    }

    usleep(500000 + rand() % 500000);

    buf[0] = 'w';
    buf[1] = (char) index;

    printf("Sending index %d\n", index);

    if (send(sockfd, &buf, sizeof(buf), 0) == -1)
    {
        perror("send");
        exit(1);
    }

    if (recv(sockfd, &buf, sizeof(buf), 0) == -1)
    {
        perror("recieve");
        exit(1);
    }

    switch ((int) buf[0])
    {
        case OK:
            puts("OK\n");
            break;
        case ALREADLY_RESERVED:
            puts("already reserved\n");
            break;
        default:
            perror("Unexpected response");
            exit(1);
    }
    usleep(2000000 + rand() % 1500000);
}

int main(int argc, char **argv)
{
    int sockfd;
    struct sockaddr_in servaddr;

    if (argc != 2)
    {
        perror("tcpcli");
        exit(1);
    }

    while (1) {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd == -1)
        {
            perror("socket");
            exit(1);
        }

        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(SERV_PORT);
        if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) == -1)
        {
            perror("inet pton");
            exit(1);
        }

        if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
        {
            perror("connection error");
            exit(1);
        }
        else
            printf("Connected\n");

        int client_id = getpid();
        if (send(sockfd, &client_id, sizeof(client_id), 0) == -1) {
            perror("send client_id");
            exit(1);
        }

        request(sockfd);
        close(sockfd);

    }

    exit(0);
}
