// FTP Server
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
#include <ctype.h>

#define FILENAMESIZE 8

void verifyNumberofArgs(int numberOfArgs)
{
    // To ensure that correct number of command line arguments are provided
	if(numberOfArgs!=5)
    {
        fflush(stdout);
        fprintf(stdout,"Incorrect Number of Command Line Arguments Provided\n.");
        exit(0);
    }
}

void verifySecretKey(int secretKey)
{
	// Terminate if the secret key falls outside the permitted range [0,65535]
	if(secretKey<0 || secretKey>65535)
	{
		fflush(stdout);
		fprintf(stdout,"Secret key is outside the range of [0,65535]. Please try with a valid secret key");
		exit(0);
	}    
}

struct sockaddr_in populateSocketAddrObject(char *ipAddress,int portNumber)
{
    struct sockaddr_in socketAddr;

    // Initialize the object with zeroes
	bzero(&socketAddr, sizeof(socketAddr));

    fprintf(stdout,"IP Address : %s\n",ipAddress);
    fprintf(stdout,"Port : %d\n",portNumber);

    // Populate the object
	socketAddr.sin_family = AF_INET; // IPv4
	socketAddr.sin_addr.s_addr = inet_addr(ipAddress); // IP Address
	socketAddr.sin_port = htons(portNumber); // Port

    return socketAddr;
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
        fprintf(stdout,"Valid IP Address.\n");
        return true;
    }

    return false;
}

void getClientSecretKeyandFileName(char clientMessage[], unsigned short *secretKey, char *filename)
{
    fprintf(stdout,"Getting Client Secret Key and File Name.\n");
    memset(secretKey,0,sizeof(unsigned short));//(*secretKey));
    memcpy(secretKey,clientMessage,2);
    fprintf(stdout,"cm: %s\n",&clientMessage[2]);
    memcpy(filename,&clientMessage[2],8);
}

void sendFileToClient(FILE *filePointer, int clientSocketFD, int blocksize)
{
    int bufferSize;
    unsigned char *bufferContent = NULL;
    bufferContent = (unsigned char*)malloc(blocksize);
    int ctr = 0;
    while((bufferSize = fread(bufferContent, sizeof(unsigned char ), blocksize, filePointer))>0)
    {
        ctr++;
        write(clientSocketFD, bufferContent, bufferSize);
        fsync(clientSocketFD);
    }

    // Send ;;EXIT;; to the client after the entire file has been sent
    fprintf(stdout,"Complete File Sent to the Client.\n");
}

void processClientRequests(int parentSocketFD, int serverSecretKey, int blocksize)
{
    struct sockaddr_in clientAddr;
    int cLen = sizeof(clientAddr);
    int clientSocketFD;
    char clientRequest[20];
    pid_t k;
    int status;
   unsigned short clientSecretKey;
	char filename[8];
    FILE *filePointer;

    // Randomize time for coin toss
    srand(time(NULL));

    // Wait till a client request is accepted
    if((clientSocketFD = accept(parentSocketFD, (struct sockaddr *) &clientAddr, &cLen)) < 0)
    {
        fprintf(stdout,"Unable to Accept Client Requests\n");
        exit(0);
    }

    if(coinToss() == 0)
    {
        fprintf(stdout, "Heads in Coin Toss!!! Unable to Process Request\n");
        return;
    }
    else
    {
        fprintf(stdout, "Tails in Coin Toss!!! Proceed Further\n");
    }

	// Check Ip
	if(isValidIP(clientAddr.sin_addr.s_addr))
    {
		fprintf(stdout, "Can accept request from the client\n");
    }
	else
    {
		fprintf(stdout, "Cannot accept client requests\n");
		close(clientSocketFD);
		return;
	}

    // All checks necessary for reading the client request have been passed till this point
    // Read Client Request
    bzero(clientRequest,20);
    int requestLength = read(clientSocketFD, clientRequest, 20);
    fprintf(stdout,"Request Length : %d\n",requestLength);
    clientRequest[requestLength] = '\0';
    bzero(filename,8);
    // Get secret key and File Name from Client Request
    getClientSecretKeyandFileName(clientRequest,&clientSecretKey,filename);


    fprintf(stdout,"Client Secret Key %d\n",clientSecretKey);
    fprintf(stdout,"Server Secret Key %d\n",serverSecretKey);
    fprintf(stdout,"Filename: %s\n",filename);

    /*if(*clientSecretKey != serverSecretKey)
    {
        fprintf(stdout, "Client's Secret Key does not match with Server's Secret Key\n");
        close(clientSocketFD);
        return;
    }*/
    k = fork();
    if(k==0)
    {
        close(parentSocketFD);
        if(filePointer = fopen(filename,"rb"))
        {
            // If file can be opened in rb mode, this means that the file exists in the directory. Such a file should sent to the client.
            
            // Send File to Client
            sendFileToClient(filePointer, clientSocketFD, blocksize);
            fclose(filePointer);

            // Close the connection
            close(clientSocketFD);
            exit(0);
        
        }
        else
        {
            // if filePointer = NULL in rb mode, then file does not exist. The client request is ignored in this case
            fprintf(stdout,"The File Requested by the Client does not Exist in the directory.");
            close(clientSocketFD);
            exit(0);
            return;
        }
    }
    else
    {
        waitpid(k, &status, 0);
    }
    close(clientSocketFD);
}


int main(int argc, char**argv)
{
    // To verify that correct number of arguments are provided from the commandline
    verifyNumberofArgs(argc);
    
    char *serverIP        = argv[1];
    int serverPort        = atoi(argv[2]);
    int serverSecretKey   = atoi(argv[3]);
    int fileReadBlockSize = atoi(argv[4]);
    int parentSocketFD, childSocketFD;
    int sockpt = 1;
    struct sockaddr_in servAddr, clientAddr;

    verifySecretKey(serverSecretKey);

    // Create Socket
    if((parentSocketFD = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stdout,"Unable to create socket\n");
        exit(0);
    }

    // Use setsockopt so that the port is freed after server is closed
	setsockopt(parentSocketFD, SOL_SOCKET, SO_REUSEADDR, (const void *)&sockpt , sizeof(int));

    // populate serverAddr object
    servAddr = populateSocketAddrObject(serverIP,serverPort);

    // Bind Socket
	if (bind(parentSocketFD, (const struct sockaddr *)&servAddr, sizeof(servAddr)) < 0) 
	{
        fprintf(stdout,"Unable to bind\n");
        exit(0);
    }

    // Listen to clients (Maximum 5 Clients Allowed)
	if (listen(parentSocketFD, 5) < 0)
	{
        fprintf(stdout,"Listen Failed\n");
        exit(0);
    }

    while(1)
    {
        processClientRequests(parentSocketFD, serverSecretKey, fileReadBlockSize);
    }

    return 0;
}
