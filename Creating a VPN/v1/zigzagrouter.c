#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>

#include "security.h"

#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

void verify_number_of_args(int number_of_arguments)
{
    // Terminate execution if incorrect number of arguments are provided
    if (number_of_arguments != 2)
    {
        fflush(stdout);
        fprintf(stdout, "Incorrect number of command line arguments provided.\nThe Client needs 1 command line arguments namely - Node Port\n");
        exit(0);
    }
}

struct sockaddr_in populate_router_address_obj(char *ipAddress, int *portNumber)
{
    fprintf(stdout, "IP Address %s\n", ipAddress);
    fprintf(stdout, "Port Number %d\n", *portNumber);

    struct sockaddr_in router_address_obj;

    // Initialize the object with zeroes
    memset(&router_address_obj, 0, sizeof(router_address_obj));

    // Populate the object
    router_address_obj.sin_family = AF_INET;                   // IPv4
    router_address_obj.sin_addr.s_addr = inet_addr(ipAddress); // IP Address
    router_address_obj.sin_port = htons(*portNumber);          // Port

    return router_address_obj;
}

void create_and_bind_to_socket(int *socket_fd, struct sockaddr_in *address_obj, char *ipAddress, int *portNumber)
{
    if ((*socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        fprintf(stdout, "socket creation failed.\n");
        exit(0);
    }

    // Populate socket object
    *address_obj = populate_router_address_obj(ipAddress, portNumber);

    // Bind the socket with the router address
    fprintf(stdout, "Binding Port %d to the router.\n", *portNumber);
    if (bind(*socket_fd, (const struct sockaddr *)address_obj, sizeof(*address_obj)) < 0)
    {
        fprintf(stdout, "bind failed for the port %d.", *portNumber);
        exit(0);
    }
}

void get_sec_and_th_node_details(char management_packet[],
                                 int *port_no_2,
                                 int *next_port_no_2,
                                 unsigned long *fw_ip,
                                 int *port_no_3,
                                 int *next_port_no_3,
                                 unsigned long *ret_ip)
{
    struct timeval timestamp;
    gettimeofday(&timestamp, NULL);
    char fw_ip_buffer[16], ret_ip_buffer[16];
    sscanf(management_packet, "%d %d %s %d %d %s", port_no_2, next_port_no_2, fw_ip_buffer, port_no_3, next_port_no_3, ret_ip_buffer);
    *fw_ip = inet_addr(fw_ip_buffer);
    *ret_ip = inet_addr(ret_ip_buffer);
    fprintf(stdout, "-----------------------------------------------------------------\n");
    fprintf(stdout, "The following values are derived from the management packet: \n");
    fprintf(stdout, "Second Port Number                        : %u\n", *port_no_2);
    fprintf(stdout, "Forwarding Port for the Second Port Number: %u\n", *next_port_no_2);
    fprintf(stdout, "Forwarding IP                            : %s\n", fw_ip_buffer);
    fprintf(stdout, "Third Port Number                        : %d\n", *port_no_3);
    fprintf(stdout, "Forwarding Port for the Third Port Number: %d\n", *next_port_no_3);
    fprintf(stdout, "Returning IP                             : %s\n", ret_ip_buffer);
    fprintf(stdout, "Timestamp                                : %ld\n", timestamp.tv_sec);
    fprintf(stdout, "-----------------------------------------------------------------\n");

}

int main(int argc, char **argv)
{
    // Verify that the correct number of arguments are provided
    verify_number_of_args(argc);

    // Initialize Node IP and port from command line arguments
    char *node_ip = get_my_ip();
    int node_port = atoi(argv[1]);
    int router_socket_fd, sec_port_socket_fd, th_port_socket_fd;
    socklen_t router_address_obj_len;
    char management_packet[128];
    char buffer[1024];
    struct sockaddr_in router_address_obj, sec_port_add_obj, th_port_add_obj, prev_router_addr_obj;
    fd_set rset;

    int port_no_2;
    int next_port_no_2;
    unsigned long fw_ip;
    int port_no_3;
    int next_port_no_3;
    unsigned long ret_ip;

    fprintf(stdout, "Command Line Parameters for the router.\n");
    fprintf(stdout, "Router IP Address: %s\n", node_ip);
    fprintf(stdout, "Router Port      : %d\n", node_port);

    create_and_bind_to_socket(&router_socket_fd, &router_address_obj, node_ip, &node_port);

    // Receive the management UDP packet
    fprintf(stdout, "Receiving the Management UDP packet.\n");
    router_address_obj_len = sizeof(prev_router_addr_obj);
    memset(management_packet, 0, 128);
    recvfrom(router_socket_fd, (char *)management_packet, 128, 0, (struct sockaddr *)&prev_router_addr_obj, &router_address_obj_len);

    // Extracting port numbers and IPs from the message
    get_sec_and_th_node_details(management_packet,
                                &port_no_2,
                                &next_port_no_2,
                                &fw_ip,
                                &port_no_3,
                                &next_port_no_3,
                                &ret_ip);



    // Binding to the forward and return port numbers
    fprintf(stdout, "Binding to the forward and return port numbers.\n");
    create_and_bind_to_socket(&sec_port_socket_fd, &sec_port_add_obj, node_ip, &port_no_2);
    create_and_bind_to_socket(&th_port_socket_fd, &th_port_add_obj, node_ip, &port_no_3);

    FD_ZERO(&rset);

    // Create Socket Object for forwarding from second node
    fprintf(stdout, "Creating Socket Object for forwarding from second node.\n");

    struct sockaddr_in sec_port_address_obj;
    // Initialize the object with zeroes
    memset(&sec_port_address_obj, 0, sizeof(sec_port_address_obj));
    // Populate the object
    sec_port_address_obj.sin_family = AF_INET;             // IPv4
    sec_port_address_obj.sin_addr.s_addr = fw_ip;          // IP Address
    sec_port_address_obj.sin_port = htons(next_port_no_2); // Port

    // Create Socket Object for forwarding from third node
    fprintf(stdout, "Creating Socket Object for forwarding from third node.\n");
    struct sockaddr_in th_port_address_obj;

    // Initialize the object with zeroes
    memset(&th_port_address_obj, 0, sizeof(th_port_address_obj));
    // Populate the object
    th_port_address_obj.sin_family = AF_INET;             // IPv4
    th_port_address_obj.sin_addr.s_addr = ret_ip;         // IP Address
    th_port_address_obj.sin_port = htons(next_port_no_3); // Port

    fprintf(stdout, "Router is ready for forwarding packets.\n");
    int maxfd = MAX(sec_port_socket_fd, th_port_socket_fd) + 1;
    printf("%d \n", maxfd);
    int flag = 0;
    while (1)
    {
        FD_SET(sec_port_socket_fd, &rset);
        FD_SET(th_port_socket_fd, &rset);

        select(maxfd, &rset, NULL, NULL, NULL);

        if (FD_ISSET(sec_port_socket_fd, &rset))
        {
            fprintf(stdout, "Forwarding the Packet.\n");
            bzero(buffer, sizeof(buffer));
            memset(&prev_router_addr_obj, 0, sizeof(prev_router_addr_obj));
            recvfrom(sec_port_socket_fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&prev_router_addr_obj, &router_address_obj_len);
            fprintf(stdout,"---------------------------------------------------------------------------------\n");
            fprintf(stdout,"Message Received from IP (while forwarding to the server): %s\n",inet_ntoa(prev_router_addr_obj.sin_addr));
            fprintf(stdout,"---------------------------------------------------------------------------------\n");            
            if (next_port_no_3 == 0 && ret_ip == 0 && flag == 0)
            {
                flag = 1;
                fprintf(stdout, "This is the first node after the client. Assigning the client IP and Port as the return path.\n");
                th_port_address_obj = prev_router_addr_obj;
            }
            fprintf(stdout, "Message from Previous Node: %s\n", buffer);
            if (prev_router_addr_obj.sin_addr.s_addr == sec_port_address_obj.sin_addr.s_addr)
            {
                fprintf(stdout, "Received the packet from the server, returning to the next node.\n");
                sendto(th_port_socket_fd, (const char *)buffer, sizeof(buffer), 0, (struct sockaddr *)&th_port_address_obj, sizeof(th_port_address_obj));
            }
            else
            {
                sendto(sec_port_socket_fd, (const char *)buffer, sizeof(buffer), 0, (struct sockaddr *)&sec_port_address_obj, sizeof(sec_port_address_obj));
            }
        }

        if (FD_ISSET(th_port_socket_fd, &rset))
        {
            fprintf(stdout, "Returning the packet to the next node.\n");
            bzero(buffer, sizeof(buffer));
            memset(&prev_router_addr_obj, 0, sizeof(prev_router_addr_obj));
            recvfrom(th_port_socket_fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&prev_router_addr_obj, &router_address_obj_len);
            fprintf(stdout,"---------------------------------------------------------------------------------\n");
            fprintf(stdout,"Message Received from IP (while returning to the client): %s\n",inet_ntoa(prev_router_addr_obj.sin_addr));
            fprintf(stdout,"---------------------------------------------------------------------------------\n");                 
            fprintf(stdout, "Message from Previous Node: %s\n", buffer);
            sendto(th_port_socket_fd, (const char *)buffer, sizeof(buffer), 0, (struct sockaddr *)&th_port_address_obj, sizeof(th_port_address_obj));
        }
    }
    close(router_socket_fd);
    close(sec_port_socket_fd);
    close(th_port_socket_fd);
    return 0;
}