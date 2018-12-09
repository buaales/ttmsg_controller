#include "ptp.h"
/* default, printf, etc */
#include <stdio.h>
/* socket stuff */
#include <sys/socket.h>
/* socket structs */
#include <netdb.h>
/* strtol, other string conversion stuff */
#include <stdlib.h>
/* string stuff(memset, strcmp, strlen, etc) */
#include <string.h>
/* signal stuff */
#include <signal.h>

#include "common.h"
#include "stdnetwork.h"

void sig_handler(int signum) {}

/* calculate master-slave difference */
static void sync_packet(int sock, struct sockaddr_in *master)
{
    char useless_buffer[FIXED_BUFFER];
    int t2[2];
    receive_packet(sock, useless_buffer, FIXED_BUFFER, t2, NULL);
    send_packet(sock, t2, sizeof(t2), NULL, master);
}

/* calculate slave-master difference */
static void delay_packet(int sock, struct sockaddr_in *master)
{
    char useless_buffer[FIXED_BUFFER];
    int t3[2];
    receive_packet(sock, useless_buffer, FIXED_BUFFER, NULL, NULL); /* wait for master to get ready */
    get_time(t3);
    send_packet(sock, t3, sizeof(t3), NULL, master);
}

/* IEEE 1588 PTP slave implementation */
static void sync_clock(int times, int sock, struct sockaddr_in *master)
{
    char useless_buffer[FIXED_BUFFER];
    int i; /* to prevent C99 error in for loop */

    send_packet(sock, (void *)"ready", 5, NULL, master);

    /* run protocol num of times determined by master */
    for (i = 0; i < times; i++)
    {
        sync_packet(sock, master);
        delay_packet(sock, master);
        receive_packet(sock, useless_buffer, FIXED_BUFFER, NULL, NULL);
    }
}

int ptp_master(int port)
{

    /* inits */
    int sock;
    struct sockaddr_in bind_addr;

    /* create socket file descriptor( AF_INET = ipv4 address family; SOCK_DGRAM = UDP; 0 = default protocol) */
    sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (unlikely(sock == -1))
    {
        ERROR("ERROR creating socket!");
        exit(EXIT_FAILURE);
    }

    /* set details for socket to bind */
    memset(&bind_addr, '\0', sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = INADDR_ANY; /* bind to all interfaces */
    /* htons = host to network byte order, necessary for universal understanding by all machines */
    bind_addr.sin_port = htons(port);

    /* bind socket */
    if (unlikely(bind(sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0))
    {
        close_socket(sock);
        ERROR("ERROR binding!\n");
        exit(EXIT_FAILURE);
    }

    /* signal handling to elegantly exit and close socket on ctrl+c */
    struct sigaction sa;
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    struct sockaddr_in addr = {0};
    char buffer[FIXED_BUFFER] = {0};
    memset(buffer, 0, sizeof(buffer));
    while (1)
    {
        receive_packet(sock, buffer, FIXED_BUFFER, NULL, &addr);
        if (strcmp(buffer, "sync") == 0)
        {
            printf("SYNC!\n ");
            send_packet(sock, (void *)"ready", 5, NULL, &addr);
            int t;
            receive_packet(sock, &t, sizeof(t), NULL, NULL);
            t = NUM_OF_TIMES;
            printf("NEW ROUND %d! \n", t);
            sync_clock(t, sock, &addr);
        }
        else
        {
            printf("Received invalid request...\n");
            send_packet(sock, (void *)HELLO, sizeof(HELLO), NULL, &addr);
        }
        printf("DONE\n");
    }
    close_socket(sock);
    return 0;
}

void* master_thread(void* arg)
{
    ptp_master((int)arg);
}