// Client side implementation of UDP client-server model
// The following reference was used as a starting point
// https://www.geeksforgeeks.org/udp-server-client-implementation-c/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>

#define PING_PARAM "pingparam.dat"
#define SRV_PAYLOAD_SIZE 4
#define CLI_PAYLOAD_SIZE 5
#define MAP_SIZE 1009 // should be prime to reduce collisions

char *make_request(char *request, int m_id, int command)
{
    sprintf(request, "%d", m_id);

    // the '\0' will get replaced with the command.
    request[CLI_PAYLOAD_SIZE - 1] = command;
    return request;
}

int get_mid_from_response(char *response)
{
    char temp[SRV_PAYLOAD_SIZE + 1];
    sprintf((char *)temp, "%c%c%c%c", response[0], response[1], response[2], response[3]);
    temp[SRV_PAYLOAD_SIZE] = '\0';
    return atol(temp);
}

int main(int argc, char *argv[])
{
    FILE *ping_param_fp;
    int sockfd, n, t, d, s;
    char request[CLI_PAYLOAD_SIZE];
    char response[SRV_PAYLOAD_SIZE];
    struct sockaddr_in servaddr, servaddr2, cliaddr;

    // maps track the start time and end time corresponding to a message id
    struct timeval start_map[MAP_SIZE], end_map[MAP_SIZE], rtt;
    socklen_t len;

    // read the configuration file for values of n, t, d and s
    ping_param_fp = fopen(PING_PARAM, "r");
    fscanf(ping_param_fp, "%d %d %d %d", &n, &t, &d, &s);
    fclose(ping_param_fp);

    // create a socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket creation failed");
        exit(1);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&servaddr2, 0, sizeof(servaddr2));

    // set server information from argv
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(argv[2]);
    servaddr.sin_port = htons(strtol(argv[3], NULL, 10));

    // set client information from argv
    cliaddr.sin_family = AF_INET; // IPv4
    cliaddr.sin_addr.s_addr = inet_addr(argv[1]);
    cliaddr.sin_port = htons(51234);

    // Bind the socket with the server address
    if (bind(sockfd, (const struct sockaddr *)&cliaddr,
             sizeof(cliaddr)) < 0)
    {
        perror("bind failed");
        exit(1);
    }

    printf("DEBUG: client port: %d\n", cliaddr.sin_port);

    for (int i = 0; i < n; i++)
    {
        gettimeofday(&start_map[s % MAP_SIZE], NULL);

        sendto(sockfd,
               (const char *)make_request(request, s, d),
               CLI_PAYLOAD_SIZE,
               MSG_CONFIRM,
               (const struct sockaddr *)&servaddr,
               sizeof(servaddr));

        // 10 sec alarm ensures the program exits
        // if the client doesn't receive from the server
        alarm(10);

        recvfrom(sockfd,
                 (char *)response,
                 SRV_PAYLOAD_SIZE,
                 MSG_WAITALL,
                 (struct sockaddr *)&servaddr2,
                 &len);

        gettimeofday(&end_map[get_mid_from_response(response) % MAP_SIZE], NULL);

        timersub(&end_map[s % MAP_SIZE], &start_map[s % MAP_SIZE], &rtt);

        printf("Time elapsed: %lf msec\n", (rtt.tv_sec * 1000) + (float)rtt.tv_usec / 1000);
        s++;

        if (n > 1)
        {
            // t specifies how many seconds the client should wait
            // before sending the next request
            sleep(t);
        }
    }

    close(sockfd);
}
