#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "common.h"

#define SLEEP_US 800000

void request(struct sockaddr_in servaddr)
{
    int index, sockfd;
    char buf[BUF_SIZE];

    while (1) {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd == -1) {
            perror("Can't socket");
            exit(1);
        }

        if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
            perror("Can't connect");
            exit(1);
        } else {
            printf("Connected\n");
        }

        buf[0] = 'r';
        if (send(sockfd, &buf, sizeof(buf), 0) == -1) {
            perror("Can't send");
            exit(1);
        }

        if (recv(sockfd, &buf, sizeof(buf), 0) == -1) {
            perror("Can't recieve");
            exit(1);
        }

        printf("Recieved seats: ");
        for (size_t i = 0; i < ARR_SIZE; i++)
            printf("%c", buf[i]);
        puts("");

        index = -1;
        for (int i = 0; i < ARR_SIZE; i++) {
            if (buf[i] != ' ') {
                index = i;
                break;
            }
        }

        if (index == -1) {
            printf("No availible seats left. Disconnecting from server\n");
            exit(1);
        }

        usleep(SLEEP_US + rand() % SLEEP_US);

        buf[0] = 'w';
        buf[1] = (char) index;

        printf("Sending index %d to server\n", index);

        if (send(sockfd, &buf, sizeof(buf), 0) == -1) {
            perror("Can't send");
            exit(1);
        }

        if (recv(sockfd, &buf, sizeof(buf), 0) == -1) {
            perror("Can't recv");
            exit(1);
        }

        switch ((int) buf[0]) {
            case OK:
                puts("Response: OK\n");
                break;
            case ALREADLY_RESERVED:
                puts("Response: already reserved\n");
                break;
            default:
                perror("Unexpected response from server");
                exit(1);
        }

        usleep(SLEEP_US + rand() % SLEEP_US);
        close(sockfd);
    }
}

int main(int argc, char **argv)
{
    struct sockaddr_in servaddr;

    if (argc != 2) {
        perror("usage: tcpcli <IPaddress>");
        exit(1);
    }

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERV_PORT);
    if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) == -1) {
        perror("Can't inet_pton");
        exit(1);
    }

    request(servaddr);

    exit(0);
}
