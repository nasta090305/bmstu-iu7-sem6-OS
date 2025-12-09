#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <netinet/in.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/wait.h>
#include "common.h"

#define READER_QUEUE 0
#define WRITER_QUEUE 1
#define READER 2
#define WRITER 3
#define SHADOW_WRITER 4

int listenfd, semid, shmid;
char *arr;

struct sembuf start_rd[] = {{READER_QUEUE, 1, 0},
                            {WRITER_QUEUE, 0, 0},
                            {SHADOW_WRITER, 0, 0},
                            {READER, 1, 0},
                            {READER_QUEUE, -1, 0}};

struct sembuf stop_rd[] = {{READER, -1, 0}};

struct sembuf start_wr[] = {{WRITER_QUEUE, 1, 0},
                            {READER, 0, 0},
                            {WRITER, -1, 0},
                            {SHADOW_WRITER, 1, 0},
                            {WRITER_QUEUE, -1, 0}};

struct sembuf stop_wr[] = {{SHADOW_WRITER, -1, 0},
                           {WRITER, 1, 0}};

void process_client(int connfd) {
    unsigned index;
    char buf[BUF_SIZE];
    int status;
    int full;

    printf("%d New connection\n", getpid());

    while (1) {
        ssize_t bytes = recv(connfd, buf, BUF_SIZE, 0);
        if (bytes <= 0) break;

        switch (buf[0]) {
            case 'r':
                semop(semid, start_rd, 5);
                for (size_t i = 0; i < ARR_SIZE; i++) buf[i] = arr[i];
                send(connfd, buf, ARR_SIZE, 0);
                semop(semid, stop_rd, 1);
                break;

            case 'w':
                index = (unsigned)buf[1];
                semop(semid, start_wr, 5);
                
                if (index < ARR_SIZE && arr[index] != ' ') {
                    arr[index] = ' ';
                    status = OK;
                    printf("%d Reserved %u\n", getpid(), index);
                } else {
                    status = (arr[index] == ' ') ? ALREADLY_RESERVED : ERROR;
                    printf("%d Failed %u\n", getpid(), index);
                }

                full = 1;
                for (size_t i = 0; i < ARR_SIZE; i++) {
                    if (arr[i] != ' ') {
                        full = 0;
                        break;
                    }
                }

                if (full) {
                    printf("%d All chars reserved\n", getpid());
                    kill(getppid(), SIGINT);
                }

                send(connfd, &status, sizeof(status), 0);
                semop(semid, stop_wr, 2);
                break;
        }
    }
    close(connfd);
}

void sigint_handler(int sig) {
    close(listenfd);
    semctl(semid, 0, IPC_RMID);
    shmctl(shmid, IPC_RMID, NULL);
    exit(0);
}

int main() {
    struct sockaddr_in servaddr, cliaddr;
    socklen_t clilen = sizeof(cliaddr);

    signal(SIGINT, sigint_handler);
    signal(SIGCHLD, SIG_IGN);

    shmid = shmget(IPC_PRIVATE, ARR_SIZE, IPC_CREAT | 0666);
    arr = shmat(shmid, NULL, 0);
    for (size_t i = 0; i < ARR_SIZE; i++) 
        arr[i] = 'a' + i;

    semid = semget(IPC_PRIVATE, 5, IPC_CREAT | 0666);
    semctl(semid, READER_QUEUE, SETVAL, 0);
    semctl(semid, WRITER_QUEUE, SETVAL, 0);
    semctl(semid, READER, SETVAL, 0);
    semctl(semid, WRITER, SETVAL, 1);
    semctl(semid, SHADOW_WRITER, SETVAL, 0);

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);

    bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
    listen(listenfd, 1024);
    printf("Server started on port %d\n", SERV_PORT);

    while (1) {
        int connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &clilen);
        if (connfd < 0) continue;

        pid_t pid = fork();
        if (pid == 0) {
            close(listenfd);
            process_client(connfd);
            exit(0);
        } else {
            close(connfd);
        }
    }
}
