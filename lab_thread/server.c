#define _GNU_SOURCE
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <netinet/in.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <time.h>
#include <sched.h>  // Include sched.h for CPU_* functions

#include "common.h"

int sfd, semid;
char arr[ARR_SIZE];

#define READER_QUEUE 0
#define WRITER_QUEUE 1
#define READER 2
#define WRITER 3
#define SHADOW_WRITER 4
#define FILENAME "serv.txt"

struct sembuf start_rd[] = {
        {READER_QUEUE, 1, 0},
        {WRITER_QUEUE, 0, 0},
        {SHADOW_WRITER, 0, 0},
        {READER, 1, 0},
        {READER_QUEUE, -1, 0}
};

struct sembuf stop_rd[] = {
        {READER, -1, 0}
};

struct sembuf start_wr[] = {
        {WRITER_QUEUE, 1, 0},
        {READER, 0, 0},
        {WRITER, -1, 0},
        {SHADOW_WRITER, 1, 0},
        {WRITER_QUEUE, -1, 0}
};

struct sembuf stop_wr[] = {
        {SHADOW_WRITER, -1, 0},
        {WRITER, 1, 0}
};

// void get_tid() {
//     int tid = syscall(SYS_gettid);
//     printf("tid = %d\n", tid);
// }

void* service(void *args) {
    int connfd = *((int *)args);
    char buf[BUF_SIZE];
    unsigned index;
    int status, full;

    int client_id;
    if (recv(connfd, &client_id, sizeof(client_id), 0) <= 0) {
        perror("recv client_id");
        close(connfd);
        pthread_exit(NULL);
    }

    int arr_empty = 0;
    while (!arr_empty) {
        if (recv(connfd, buf, BUF_SIZE, 0) <= 0) break;

        switch (buf[0]) {
            case 'r':
                if (semop(semid, start_rd, 5) == -1) {
                    perror("semop");
                    exit(EXIT_FAILURE);
                }
                for (size_t i = 0; i < ARR_SIZE; i++) {
                    buf[i] = arr[i];
                }
                if (send(connfd, &buf, sizeof(buf), 0) == -1) {
                    perror("send");
                    exit(EXIT_FAILURE);
                }

                if (semop(semid, stop_rd, 1) == -1) {
                    perror("semop");
                    exit(EXIT_FAILURE);
                }
                break;

            case 'w':
                index = (int) buf[1];
                if (semop(semid, start_wr, 5) == -1) {
                    perror("semop");
                    exit(EXIT_FAILURE);
                }

                if (index < ARR_SIZE && arr[index] != ' ') {
                    arr[index] = ' ';
                    status = OK;
                    printf("[pid = %d, tid = %d, self = %d, cpuid = %d] reserved letter %u\n", client_id, (int)syscall(SYS_gettid), (int)pthread_self(), sched_getcpu(), index);
                } else {
                    if (arr[index] == ' ') {
                        status = ALREADLY_RESERVED;
                        printf("[pid = %d, tid = %d, self = %d, cpuid = %d] failed to reserve letter %u\n", client_id, (int)syscall(SYS_gettid), (int)pthread_self(), sched_getcpu(), index);
                    } else {
                        status = ERROR;
                        printf("[pid = %d, tid = %d, self = %d, cpuid = %d] server received invalid letter number %u\n", client_id, (int)syscall(SYS_gettid), (int)pthread_self(), sched_getcpu(), index);
                    }
                }

                full = 1;
                for (size_t i = 0; full && i < ARR_SIZE; i++) {
                    if (arr[i] != ' ')
                        full = 0;
                }

                if (full) {
                    printf("[pid = %d, tid = %d, self = %d, cpuid = %d] All letters reserved\n", client_id, (int)syscall(SYS_gettid), (int)pthread_self(), sched_getcpu());
                    arr_empty = 1;
                }

                if (send(connfd, &status, sizeof(status), 0) == -1) {
                    perror("send");
                    exit(EXIT_FAILURE);
                }

                if (semop(semid, stop_wr, 2) == -1) {
                    perror("semop");
                    exit(EXIT_FAILURE);
                }
                break;
        }
    }
    close(connfd);
    //sleep(10);
    pthread_exit(NULL);
}


void sigint_handler() {
    close(sfd);
    semctl(semid, 5, IPC_RMID, NULL);
    exit(EXIT_SUCCESS);
}

int main(void) {
    int listenfd, connfd;
    socklen_t clilen;
    struct sockaddr_in cliaddr, servaddr;
    pthread_attr_t attr;
    pthread_t p_thread;

    signal(SIGINT, sigint_handler);

    semid = semget(IPC_PRIVATE, 5, IPC_CREAT | 0666);
    if (semid == -1) {
        perror("semget");
        exit(EXIT_FAILURE);
    }
    if (semctl(semid, READER_QUEUE, SETVAL, 0) == -1) {
        perror("semctl");
        exit(EXIT_FAILURE);
    }
    if (semctl(semid, WRITER_QUEUE, SETVAL, 0) == -1) {
        perror("semctl");
        exit(EXIT_FAILURE);
    }
    if (semctl(semid, READER, SETVAL, 0) == -1) {
        perror("semctl");
        exit(EXIT_FAILURE);
    }
    if (semctl(semid, WRITER, SETVAL, 1) == -1) {
        perror("semctl");
        exit(EXIT_FAILURE);
    }
    if (semctl(semid, SHADOW_WRITER, SETVAL, 0) == -1) {
        perror("semctl");
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < ARR_SIZE; i++)
        arr[i] = (char)('a' + i);

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);

    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(listenfd, 1024) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("listening on port %d\n", SERV_PORT);

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    while (1) {
        clilen = sizeof(cliaddr);
        connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);

        if (pthread_create(&p_thread, &attr, service, &connfd) != 0) {
            perror("pthread_create");
        }

    }
    pthread_attr_destroy(&attr);
    semctl(semid, 5, IPC_RMID, NULL);
    return 0;
}
