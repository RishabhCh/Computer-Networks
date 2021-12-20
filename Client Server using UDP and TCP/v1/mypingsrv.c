#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

int main(int argc, char **argv)
{
	if(argc!=3)
    {
        fflush(stdout);
        fprintf(stdout,"Incorrect Number of Command Line Arguments Provided\n. Please provide in following format: mypingsrv.bin server-IP server-Port\n");
        exit(0);
    }
   char buffer[100];
   char *args[3];
   int socketFileD, recv, recvLen;
   struct sockaddr_in servAddr, clientAddr;
   pid_t k;
   
   socketFileD = socket(AF_INET,SOCK_DGRAM,0);
   if(socketFileD < 0)
   {
	fprintf(stdout,"Socket Creation Failed");
	exit(EXIT_FAILURE);
   }

   memset(&servAddr, 0, sizeof(servAddr));
   memset(&clientAddr, 0, sizeof(clientAddr));

   servAddr.sin_family      = AF_INET;
   servAddr.sin_addr.s_addr = inet_addr(argv[1]);
   servAddr.sin_port        = htons(atoi(argv[2]));

   if(bind(socketFileD, (const struct sockaddr *)&servAddr, sizeof(servAddr)) <0)
   {
        fprintf(stdout,"Bind Failed");
   }
   
   recvLen = sizeof(clientAddr);

   while(1)
   {
	recv = recvfrom(socketFileD, (char *)buffer, 100, 0, ( struct sockaddr *) &clientAddr, &recvLen);
	buffer[recv] = '\0';
	if(recv>0)
   {
	fflush(stdout);
   fprintf(stdout,"Message %s recvd \n",buffer);
	if(buffer[4] == 0)
	{
		buffer[4] = '\0';
                sendto(socketFileD, (char *)buffer, strlen(buffer), 0, ( struct sockaddr *) &clientAddr, recvLen);
                fprintf(stdout,"Message %s returned by server for D = 0 \n",buffer);

	}
	else if(buffer[4]>=1 && buffer[4]<=5)
	{
		k = fork();
		if(k==0)
		{
		    sleep(buffer[4]);
		    buffer[4] = '\0';
		    sendto(socketFileD, (char *)buffer, strlen(buffer), 0, ( struct sockaddr *) &clientAddr, recvLen);
		    fprintf(stdout,"Message %s returned by server\n",buffer);
          exit(0);
		}
	}
	else if(buffer[4]==99)
	{
		fprintf(stdout,"Instruction to terminate the server\n");
		exit(1);
	}
	else
	{
		fprintf(stdout,"Incorrect value for byte D\n");
	}}
   }

   return 0;
}


