// Server side implementation of UDP client-server model
// The following reference was used as a starting point
// https://www.geeksforgeeks.org/udp-server-client-implementation-c/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define CLI_PAYLOAD_SIZE 5
#define SRV_PAYLOAD_SIZE 4

int make_response(char *response, char *request)
{
    pid_t k;
    int status;
    char d = request[CLI_PAYLOAD_SIZE - 1];

    // check if the requested command is valid
    if (d != 99 && d < 0 && d > 5)
    {
        // invalid request, ignore it
        return 0;
    }

    if (d == 99)
    {
        exit(1);
    }

    // d is between 1 and 5. Sleep for d seconds in child process
    if (d > 0)
    {
        k = fork();
        if (k == 0)
        {
            sleep(d);
        }
        else
        {
            // parent code
            waitpid(k, &status, 0);
        }
    }

    // copy the MID from the request to response
    for (int i = 0; i < SRV_PAYLOAD_SIZE; i++)
    {
        response[i] = request[i];
    }
    return 1;
}

int main(int argc, char *argv[])
{
    int sockfd;
    char request[CLI_PAYLOAD_SIZE];
    char response[SRV_PAYLOAD_SIZE];
    struct sockaddr_in servaddr, cliaddr;
    socklen_t len;

    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket creation failed");
        exit(1);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    // set server information from argv
    servaddr.sin_family = AF_INET; // IPv4
    servaddr.sin_addr.s_addr = inet_addr(argv[1]);
    servaddr.sin_port = htons(strtol(argv[2], NULL, 10));

    // Bind the socket with the server address
    if (bind(sockfd, (const struct sockaddr *)&servaddr,
             sizeof(servaddr)) < 0)
    {
        perror("bind failed");
        exit(1);
    }

    len = sizeof(cliaddr);

    while (1)
    {

        recvfrom(sockfd, (char *)request, CLI_PAYLOAD_SIZE,
                 MSG_WAITALL, (struct sockaddr *)&cliaddr,
                 &len);
   
        if (make_response(response, request))
        {
            sendto(sockfd, (const char *)response, SRV_PAYLOAD_SIZE,
                   MSG_CONFIRM, (const struct sockaddr *)&cliaddr,
                   sizeof(cliaddr));
        }
    }
}
