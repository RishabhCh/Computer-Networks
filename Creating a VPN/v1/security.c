#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>

#include "security.h"

unsigned int bbdecode(unsigned int x, unsigned int prikey)
{
    return x ^ prikey;
}

unsigned int bbencode(unsigned int y, unsigned int pubkey)
{
    return y ^ pubkey;
}

unsigned long get_ip_int(char *ip_address)
{
    struct sockaddr_in sa;

    if (inet_pton(AF_INET, ip_address, &(sa.sin_addr)) != 1)
    {
        fprintf(stderr, "could not convert address %s\n", ip_address);
        exit(1);
    }
    return sa.sin_addr.s_addr;
}

int *get_acl()
{
    int *map = (int *)malloc(sizeof(int) * MAP_SIZE);

    FILE *fp;
    char ip[17];
    unsigned long ip_int;
    int pubkey;

    fp = fopen("acl.dat", "r");
    if (fp == NULL)
    {
        fprintf(stderr, "could not open acl.dat file\n");
        exit(1);
    }

    while (fscanf(fp, "%s %d", (char *)ip, &pubkey) > 0)
    {
        ip_int = get_ip_int(ip);
        *(map + (ip_int % MAP_SIZE)) = pubkey;
    }
    fclose(fp);
    return map;
}

// reference: https://www.geeksforgeeks.org/c-program-display-hostname-ip-address/
char *get_my_ip()
{
    char hostbuffer[256];
    char *ipbuffer;
    struct hostent *host_entry;

    gethostname(hostbuffer, sizeof(hostbuffer));
    host_entry = gethostbyname(hostbuffer);
    ipbuffer = inet_ntoa(*((struct in_addr *)host_entry->h_addr_list[0]));
    return ipbuffer;
}
