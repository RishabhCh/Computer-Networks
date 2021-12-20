#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>


struct packetdtls
{
	int MID;
	double sending_time;
};

void sig_handler(int signum)
{
	fprintf(stdout,"Alarm Timeout\n");
	exit(0);
}

double get_time_in_msec()
{
	struct timeval ctime;
	gettimeofday(&ctime,NULL);
	return ((ctime.tv_sec*1000)+(ctime.tv_usec/1000));
}

int main(int argc, char **argv) 
{

	if(argc!=4)
    {
        fflush(stdout);
        fprintf(stdout,"Incorrect Number of Command Line Arguments Provided\n. Please provide in following format: mypingcli.bin client-IP server-IP server-Port\n");
        exit(0);
    }

   int socketFileD, T, D, S, recv, recvLen, N;
   struct sockaddr_in servAddr, clientAddr;
   FILE *fptr;
   char buffer[100], sN[5], sT[5], sD[5], sS[5];
   char MID[10];
   struct timeval current_time;
   struct packetdtls pkdtl[7];
   double timediff;

   // Initializing MID for the packet details
   for(int i=0;i<7;i++)
   {
	   pkdtl[i].MID = 0;
   }
   // Registering signal handler for alarm
   signal(SIGALRM,sig_handler);

   fflush(stdout);
   fflush(stdin);

   // open pingparam file
   fptr = fopen("pingparam.dat","r");
   if(fptr == NULL)
   {
	fprintf(stdout,"Error in opening the pingparam file");
	exit(1);
   }
   
   // reading values for N, T, D and S
   fscanf(fptr,"%d",&N);
   fscanf(fptr,"%d",&T);
   fscanf(fptr,"%d",&D);
   fscanf(fptr,"%d",&S);

   // Getting string values of N, T, D and S
   sprintf(sN,"%d",N);
   sprintf(sT,"%d",T);
   sprintf(sD,"%d",D);

   // closing pingparam file
   fclose(fptr);

   // Creating a socket
   socketFileD = socket(AF_INET,SOCK_DGRAM,0);

   if(socketFileD < 0)
   {
   	fprintf(stdout,"Socket Creation Failed");
	exit(1);
   }

   // Initialising client and server objects
   memset(&servAddr, 0, sizeof(servAddr));
   memset(&clientAddr, 0, sizeof(clientAddr));

   // Assigning values to the client object
   clientAddr.sin_family      = AF_INET;            // IPv4 address
   clientAddr.sin_addr.s_addr = inet_addr(argv[1]); // IP address provided by the user
   clientAddr.sin_port        = htons(0);     // 0 to assign an available port

   // Assigning values to the server object
   servAddr.sin_family      = AF_INET;            // IPv4 address
   servAddr.sin_addr.s_addr = inet_addr(argv[2]); // IP address provided by the user
   servAddr.sin_port        = htons(atoi(argv[3]));     // 0 to assign an available port

   // Binding client IP
   if(bind(socketFileD, (const struct sockaddr *)&clientAddr, sizeof(clientAddr)) <0)
   {
        fprintf(stdout,"Bind Failed for Client\n");
	close(socketFileD);
	exit(1);
   }
   int cnt = 0;
   int num_packets_left = N;
   // Loop to send packets to server
   double packet_send_time=0;
   while(1)
   {

	// send operation should be done only if the provided time delay has passed
	if(num_packets_left>0 && (get_time_in_msec()-packet_send_time) >= (T * 1000))
	{
		pkdtl[cnt].MID = S;

        	sprintf(sS,"%d",S);

	        strcpy(MID,sS);

	        // Incrementing message ID for the next iteration
		S = S+1;

      MID[4]=D;
		MID[5]='\0';

		// recording sending time for packet
		pkdtl[cnt].sending_time = get_time_in_msec();
		packet_send_time = pkdtl[cnt].sending_time;

		// Sending the message to server
		sendto(socketFileD, (const char *)MID, strlen(MID),0, (const struct sockaddr *) &servAddr,sizeof(servAddr));

		cnt++;
		num_packets_left--;
		fprintf(stdout,"ping with packet : %s\n",MID);
      
      // Wait 10 seconds after laast send
      if(num_packets_left == 0)
      {
                    alarm(10);
      }
	}

	recvLen = sizeof(servAddr);
	memset(buffer, 0, sizeof(buffer));

	recv = recvfrom(socketFileD, (char *)buffer, 100, MSG_DONTWAIT, (struct sockaddr *) &servAddr, &recvLen);

	if(recv > 0)
	{
	    fprintf(stdout,"Received a packet");
	    buffer[recv] = '\0';
	    
	    for(int i=0;i<7;i++)
	    {
		if(pkdtl[i].MID == atoi(buffer))
		{
		    
		    fprintf(stdout,"Received Packet %d from server\n",pkdtl[i].MID);

		    timediff = get_time_in_msec() - pkdtl[i].sending_time;
		    fprintf(stdout,"Round Trip Time for the packet %d is %lf\n", pkdtl[i].MID, timediff);
		    break;		    
		}
	    }
	    // Wait for 10 sec after the last receive
	    if(num_packets_left == 0)
            {
                    alarm(10);
            }
	}

   }

   return 0;
}
