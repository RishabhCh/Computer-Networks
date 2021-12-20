// TCP Client code.
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>
#include <time.h>
#include <sys/types.h> 
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void alarm_handler(int sig)
{
    // No action required here when alarm goes off
}

int main(int argc, char **argv) 
{

	if(argc!=3)
    {
        fflush(stdout);
        fprintf(stdout,"Incorrect Number of Command Line Arguments Provided\n. Please provide in following format: recommandserver.bin server-IP server-Port\n");
        exit(0);
    }

    int psFd; 
    char buffer[1024], buf[100]; 
    struct sockaddr_in servAddr;
    int ctr = 0; 

    bzero((char *) &servAddr, sizeof(servAddr));
    servAddr.sin_family = AF_INET; // IPv4
    servAddr.sin_addr.s_addr = inet_addr(argv[1]); // IP Address
    servAddr.sin_port = htons(atoi(argv[2])); // Port

    // Creating the socket
	if ((psFd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
  	{
        fflush(stdout);
        fprintf(stdout,"Unable to create socket\n");
        exit(0);
    }

  	// Connect to server
  	if (connect(psFd, (struct sockaddr*)&servAddr, sizeof(servAddr)) < 0) 
    	{
        fflush(stdout);
        fprintf(stdout,"Unable to connect\n");
        exit(0);
    }

    // Main code logic
    while(1)
    {
        // Flush Buffers
        fflush(stdout);
		fflush(stdin);
		// prompt
		fprintf(stdout,"> ");

		bzero(buf, 100);
        // User input command
		fgets(buf, 100, stdin);

		buf[strlen(buf)-1] = '\0';

		// Alarm before sending the message
		siginterrupt(SIGALRM, 1);

		signal(SIGALRM, alarm_handler);

		alarm(2);

        // Increment number of tries
		ctr++;
		
		
		if (write(psFd, buf, strlen(buf)) < 0)
        { 
			perror("Error is sending message\n");
			continue;
		}
      
		fprintf(stdout,"Message sent to server %d time(s)",ctr);
	
		while(1){
			bzero(buffer, 1024);

			ssize_t msgLen = read(psFd, buffer, 1024);

			fflush(stdout);

			if (msgLen < 0)
            {
				if (errno != EINTR) 
					perror("Error is reading response\n");
				else 
                {
					// Alarm expired, need to reconnect for the next try 

                    // Close socket as there is a need to reconnect	
                    close(psFd);
 
  	                // Reconnect only till 3rd attempt
  	                if(ctr < 3)
                      {
    
    // Creating the socket
	if ((psFd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
  	{
        fflush(stdout);
        fprintf(stdout,"Unable to create socket\n");
    }

  	// Connect to server
  	if (connect(psFd, (struct sockaddr*)&servAddr, sizeof(servAddr)) < 0) 
    	{
        fflush(stdout);
        fprintf(stdout,"Unable to connect\n");
        }

    	// Set the alarm for 2 seconds before sending the message
    	alarm(2);
    	write(psFd, buf, strlen(buf));
      		
    
        // Increment number of tries
    	ctr++;

    	fprintf(stdout, "No response from server, trying again\n");
  	}
  	else
    {
    	fprintf(stdout, "No response for 2 requests, terminating program\n");
    	exit(EXIT_FAILURE);
  	}
					continue;
				}
			}
			else if (msgLen == 0)
            {
                // Alarm reset as message arrived
				alarm(0);
				ctr = 0;
				break;
			}
			else 
            {
                // Alarm reset as message arrived
				alarm(0);
				ctr = 0;
                // This is done to parse long messages, it will keep writing till 'EXIT' is encountered, the below code removes 'EXIT' before printing to stdout

				const char *end_message = &buffer[strlen(buffer) - strlen("EXIT")];

				if(strcmp(end_message, "EXIT") == 0) 
                {
                    break;
                }

				fprintf(stdout, "%s\n", buffer);
				fflush(stdout);
			}
		}
      
    }
    // Close Socket
    close(psFd);

    return 0;
}