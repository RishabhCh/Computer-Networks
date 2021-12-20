#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <errno.h>
#include <netdb.h>

#include "security.h"
#define ZZOVERLAY_DAT "zzoverlay.dat"

int main(int argc, char *argv[])
{
    FILE *fp = fopen(ZZOVERLAY_DAT, "r");
    int n, sockfd;
    struct sockaddr_in servaddr, cliaddr;
    memset(&cliaddr, 0, sizeof(cliaddr));
    char router_host[20], temp1[20], temp2[20], request[128];
    int router_port, temp1_src, temp1_dst, temp2_src, temp2_dst;
    // socklen_t len;
    struct timeval timestamp;

    char *ip = get_my_ip();

    memset(&cliaddr, 0, sizeof(cliaddr));
    cliaddr.sin_family = AF_INET;
    cliaddr.sin_addr.s_addr = inet_addr(ip);
    cliaddr.sin_port = 0;

    printf("zigzagconf IP: %s\n", ip);


    // create a socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket creation failed");
        exit(1);
    }

    // Bind the socket with the server address
    if (bind(sockfd, (const struct sockaddr *)&cliaddr,
             sizeof(cliaddr)) < 0)
    {
        perror("bind failed");
        exit(1);
    }

    fscanf(fp, "%d", &n);
    while (n--)
    {
        fscanf(fp, "%s %d", router_host, &router_port);
        fscanf(fp, "%d %d %s", &temp1_src, &temp1_dst, temp1);
        fscanf(fp, "%d %d %s", &temp2_src, &temp2_dst, temp2);

        memset(&servaddr, 0, sizeof(servaddr));

        servaddr.sin_family = AF_INET; // IPv4
        servaddr.sin_addr.s_addr = inet_addr(router_host);
        servaddr.sin_port = htons(router_port);

        sprintf(request, "%d %d %s %d %d %s", temp1_src, temp1_dst, temp1, temp2_src, temp2_dst, temp2);

        sendto(sockfd,
               request,
               sizeof(request),
               MSG_CONFIRM,
               (const struct sockaddr *)&servaddr,
               sizeof(servaddr));

        gettimeofday(&timestamp, NULL);
        printf("timestamp: %ld destination: %s:%d payload: %s\n", timestamp.tv_sec, router_host, router_port, request);
    }
    fclose(fp);
}
