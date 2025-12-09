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

void get_tid() {
    int tid = syscall(SYS_gettid);
    printf("tid = %d, tid self = %d\n", tid, abs((int)pthread_self()));
}

void* service(void *args) {
    int connfd = *((int *)args);
    char buf[BUF_SIZE];
    unsigned index;
    int status, full;

    printf("new connection\n");

    int client_id;
    if (recv(connfd, &client_id, sizeof(client_id), 0) <= 0) {
        perror("recv client_id");
        close(connfd);
        pthread_exit(NULL);
    }
    printf("Client connected with ID: %d\n", client_id);

    get_tid();

    while (1) {
        if (recv(connfd, buf, BUF_SIZE, 0) <= 0) {
            printf("%d finished\n", client_id);
            break;
        }

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
                    printf("%d Client reserved letter %u\n", client_id, index);
                } else {
                    if (arr[index] == ' ') {
                        status = ALREADLY_RESERVED;
                        printf("%d Client failed to reserve letter %u\n", client_id, index);
                    } else {
                        status = ERROR;
                        printf("%d Server received invalid letter number %u\n", client_id, index);
                    }
                }

                full = 1;
                for (size_t i = 0; full && i < ARR_SIZE; i++) {
                    if (arr[i] != ' ')
                        full = 0;
                }

                if (full) {
                    printf("%d All letters reserved\n", client_id);
                    kill(getppid(), SIGINT);
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
    sleep(10);
    pthread_exit(NULL);
}


void sigint_handler() {
    close(sfd);
    semctl(semid, 5, IPC_RMID, NULL);
    exit(EXIT_SUCCESS);
}

int main(void) {
    int listenfd;
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
        unsigned long t_start = clock();
        int connfd_ptr = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);

        if (pthread_create(&p_thread, &attr, service, &connfd_ptr) != 0) {
            perror("pthread_create");
        }
        unsigned long t_end = clock();
        unsigned long elapsed_time = t_end - t_start;

        FILE *file = fopen(FILENAME, "a"); 
        if (file) {
            fprintf(file, "%lu\n", elapsed_time);
            fclose(file);
        } else {
            perror("fopen failed");
        }
    }
    pthread_attr_destroy(&attr);
    semctl(semid, 5, IPC_RMID, NULL);
    return 0;
}
