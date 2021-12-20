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
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>

void parsingfunction(char buf[],char *delimiter, char *args[])
{
	int j=0;
        char *ptr;
	ptr = strtok(buf,delimiter);                  // Storing the location of first token
        // Loop to parse the command line string string till the end
        while(ptr != NULL)
        {
           args[j]=ptr;                         // Storing the location of the token to the pointer array
           ptr = strtok(NULL,delimiter);              // Getting the location of the next token
           j = j+1;                             // Incrementing the index for the pointer array
        }
        args[j] = NULL;                         // Assigning NULL to the last index of the pointer array to mark the end of string

}	

int coinToss()
{
    return rand() % 2;
}

bool isValidIP(unsigned long ip)
{
    int byte1, byte2, byte3;
    byte1 = (ip >> (8*0)) & 0xff;
    byte2 = (ip >> (8*1)) & 0xff;
    byte3 = (ip >> (8*2)) & 0xff;

    if(byte1 == 128 && byte2 == 10 && (byte3 == 25|| byte3 == 112))
    {
        fprintf(stdout,"Valid IP Address");
        return 1;
    }
    return 0;
}

int main(int argc, char **argv) {

    // Randomize for coin toss
    srand(time(NULL));
	// To ensure that correct number of command line arguments are provided
	if(argc!=3)
    {
        fflush(stdout);
        fprintf(stdout,"Incorrect Number of Command Line Arguments Provided\n. Please provide in following format: recommandserver.bin server-IP server-Port\n");
        exit(0);
    }

    pid_t k;
	char buff[1024];
	int psFd, csFd; 
	struct sockaddr_in servAddr, clientAddr;
	
    int cLen = sizeof(clientAddr);
		
	// Creating the socket
	if ((psFd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    {
        fprintf(stdout,"Unable to create socket\n");
        exit(0);
    }
		
	int sockpt = 1;
	setsockopt(psFd, SOL_SOCKET, SO_REUSEADDR, (const void *)&sockpt , sizeof(int)); // To terminate the server after use


	bzero(&servAddr, sizeof(servAddr));

	servAddr.sin_family = AF_INET; // IPv4
	servAddr.sin_addr.s_addr = inet_addr(argv[1]); // IP Address
	servAddr.sin_port = htons(atoi(argv[2])); // Port

	// Bind socket
	if (bind(psFd, (const struct sockaddr *)&servAddr, sizeof(servAddr)) < 0) 
		{
        fprintf(stdout,"Unable to bind\n");
        exit(0);
    }


	// maximum 5 clients allowed using listen
	if (listen(psFd, 5) < 0) // allow 5 connection requests to queue up  
	{
        fprintf(stdout,"Listen Failed\n");
        exit(0);
    }
    

	// Main logic of the code
	while (1) {
	
		// Wait till the connection req
		csFd = accept(psFd, (struct sockaddr *) &clientAddr, &cLen);
		if (csFd < 0) 
		{
        fprintf(stdout,"Accept Failed\n");
        exit(0);
    }

        // Coin Toss
		if(coinToss() == 0)
			fprintf(stdout, "Heads in coin toss!!! Unable to process request\n");
		else{
			fprintf(stdout, "Tails in coin Toss!!! Proceed further\n");
			continue;
		}
		
		// Check Ip
		if(isValidIP(clientAddr.sin_addr.s_addr)==true)
			fprintf(stdout, "Can accept request from the client\n");
		else
        {
			fprintf(stdout, "Cannot accept client requests\n");
			close(csFd);
			continue;
		}

        // On correct coin toss and IP, converse with client
        k = fork();
		if (k == 0)
        {
			close(psFd);

			while(1)
            {
				bzero(buff, 1024);
				int msgSize = read(csFd, buff, 1024);
				
				fprintf(stdout, "Message from Client is %s \n", buff);
				fflush(stdout);
                
				// execute the linux command
				int status;
	            pid_t k1;
	            char* args[10]; 
	
	            k1 = fork();
	            if (k1 == 0) 
                {
		            parsingfunction(buff, " ", args);
		            dup2(csFd, 1);
		            execvp(args[0], args);
		            exit(0);
	            }
	            else 
                {
		            waitpid(k1, &status, 0);
	            }
				bzero(buff, 1024);
				strcpy(buff,"EXIT\0");
				write(csFd, buff, strlen(buff));
			}
		}
	}
	
	close(csFd);
    return 0;
}