// Server
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

void alarm_handler(int sig)
{
    // No action required here when alarm goes off
}

void verifyNumberOfArgs(int numberOfArguments)
{
	// Terminate execution if incorrect number of arguments are provided
	if(numberOfArguments != 7)
	{
		fflush(stdout);
		fprintf(stdout,"Incorrect number of command line arguments provided.\nThe Client needs 6 command line arguments namely - Server IP, Server Port, Filename, Secret-Key, Blocksize, Windowsize.\n");
		exit(0);
	}
}

bool validateSecretKey(int *secretKey)
{
	if((*secretKey) < 0 || (*secretKey) > 65535)
        return false;

	return true;
}

struct sockaddr_in populateSocketAddrObject(char *ipAddress,int portNumber)
{
    struct sockaddr_in socketAddr;

    // Initialize the object with zeroes
    memset(&socketAddr, 0, sizeof(socketAddr));

    // Populate the object
	socketAddr.sin_family      = AF_INET; // IPv4
	socketAddr.sin_addr.s_addr = inet_addr(ipAddress); // IP Address
	socketAddr.sin_port        = htons(portNumber); // Port

    return socketAddr;
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

void getClientSecretKeyandFileName(unsigned char clientMessage[], unsigned short *secretKey, char *filename)
{
    memset(secretKey,0,2); // Initializing secret key with 0
    memcpy(secretKey,clientMessage,2);
    memset(filename,0,8); // Initializing filename with 0
    memcpy(filename,&clientMessage[2],8);
    filename[8]='\0';
}

void processClientRequest(char *fileName,struct sockaddr_in *cliAddr, char *ipAddress, int *timeout, int *windowSize, int *blockSize)
{
    int sockfd, bufferSize, totalSize = 0, fileSize, ctr = 0, iack, msgLen,i;
    struct sockaddr_in servAddr;
    int addrLen = sizeof(servAddr);
    FILE *filePointer;
    char windowBuffer[64][1471];
    struct itimerval timer;
    char ack;
    bool eof = false;
    bool isRetransmit = false;

    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = *timeout;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;
    // Create Socket for the current Client
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
    {
        perror("socket creation failed");
        exit(0);
    }
      
    // Populating server information
    servAddr = populateSocketAddrObject(ipAddress, 0);
      
    // Bind the socket with the server address
    if (bind(sockfd, (const struct sockaddr *)&servAddr, sizeof(servAddr)) < 0)
    {
        perror("bind failed");
        exit(0);
    }

    filePointer = fopen(fileName,"rb");
    if(filePointer == NULL)
    {
        fprintf(stdout,"File Not Found.\n");
        exit(0);
    }

    // calculating file Size
    fseek(filePointer, 0, SEEK_END);
    fileSize = ftell(filePointer);
    // Returning file Pointer to its place
    fseek(filePointer, 0, SEEK_SET);

    while(1)
    {
        memset(windowBuffer,0,sizeof(windowBuffer));
        timer.it_value.tv_sec = (*timeout)/1000000;
        timer.it_value.tv_usec = *timeout;
        timer.it_interval = timer.it_value;
        siginterrupt(SIGALRM, 1);
	    signal(SIGALRM, alarm_handler);
        setitimer(ITIMER_REAL,&timer,NULL);

        for(i=0;i<(*windowSize);i++)
        {
            windowBuffer[i][0] = (char)(ctr++);
            bufferSize = fread(&windowBuffer[i][1],1,(*blockSize),filePointer);
            totalSize = totalSize + bufferSize;

            if(totalSize == fileSize)
            {
                if(bufferSize == (*blockSize))
                {
                    bufferSize = bufferSize + 1;
                    windowBuffer[i][bufferSize] = '?';
                }
                eof = true;
                sendto(sockfd, (const char*)&windowBuffer[i][0],bufferSize+1,0,(const struct sockaddr *)cliAddr,addrLen);
                break;
            }
            sendto(sockfd, (const char*)&windowBuffer[i][0],bufferSize+1,0,(const struct sockaddr *)cliAddr,addrLen);
        }
        msgLen = recvfrom(sockfd, (char *)&ack, 1, 0, (struct sockaddr *)cliAddr, &addrLen);

        if(msgLen > 0)
        {
            timer.it_value.tv_sec = 0;
            timer.it_value.tv_usec = 0;
            timer.it_interval = timer.it_value;
            setitimer(ITIMER_REAL,&timer,NULL);
            iack = (int)ack;

            if(iack == ctr - 1)
            {
                if(eof == true)
                {
                    fprintf(stdout,"File Completely Read.\n");
                    break;
                }
            }
            else
            {
                isRetransmit = true;
            }
        }

        if(i!=(*windowSize))
        {
            i++;
        }

        // retransmit
        if((errno == EINTR)||(isRetransmit == true))
        {
            int attempts = 0;

            while(attempts++ < 5){
            timer.it_value.tv_sec = (*timeout)/1000000;
            timer.it_value.tv_usec = *timeout;
            timer.it_interval = timer.it_value;
            siginterrupt(SIGALRM, 1);
	        signal(SIGALRM, alarm_handler);
            setitimer(ITIMER_REAL,&timer,NULL);

            for(int j=0;j<i;j++)
            {
                if(j==i-1)
                {
                    sendto(sockfd, (const char *)&windowBuffer[i][0], (bufferSize+1), 0, (const struct sockaddr *) cliAddr, addrLen);                    
                }
                else
                {
                    sendto(sockfd, (const char *)&windowBuffer[i][0], (blockSize+1), 0, (const struct sockaddr *) cliAddr, addrLen);
                }
                // If ack comes while sending (code not written)
			}
            msgLen = recvfrom(sockfd,(char*)&ack,1,0,(struct sockaddr *)cliAddr, &addrLen);
            if(msgLen > 0)
            {
                msgLen = 0;
                iack = (int)ack;
                if(iack == ctr-1)
                {
                    timer.it_value.tv_sec = 0;
                    timer.it_value.tv_usec = 0;
                    timer.it_interval = timer.it_value;
                    setitimer(ITIMER_REAL,&timer,NULL);
                    break;
                }
            }
            }
        }
    if(ctr == 2*(*windowSize))
    {
        ctr = 0;
    }
    }
    close(filePointer);

}

int main(int argc, char **argv)
{
    // Verify that correct number of command line arguments are provided
    verifyNumberOfArgs(argc);

    // Initializing Parameters
    char *serverIP = argv[1];
    int serverPort = atoi(argv[2]);
    int secretKey  = atoi(argv[3]);
    int blockSize  = atoi(argv[4]);
    int windowSize = atoi(argv[5]);    
    int timeout    = atoi(argv[6]);    
    struct sockaddr_in servAddr, cliAddr;
    int sockfd;
    int clientAddrLen, msgLen;
    unsigned char clientRequest[10];
    char *fileName;
    unsigned short clientSecretKey;  
    pid_t k;

    fprintf(stdout,"Command Line Parameters: \n");
    fprintf(stdout,"Server IP Address: %s\n",serverIP);
    fprintf(stdout,"Server Port      : %d\n",serverPort);
    fprintf(stdout,"Secret Key       : %d\n",secretKey);
    fprintf(stdout,"Block Size       : %d\n",blockSize);
    fprintf(stdout,"Window Size      : %d\n",windowSize);    
    fprintf(stdout,"Timeout          : %d\n",timeout);

    if(validateSecretKey(&secretKey)==false)
    {
        fprintf(stdout,"Invalid Secret Key. Please provide valid secret key\n");
        exit(0);
    }

    if(blockSize > 1471)
    {
        fprintf(stdout,"Block size exceedes the maximum block size. Please provide block size < 1471\n");
        exit(0);
    }

    // Creating socket file descriptor
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) 
    {
        perror("socket creation failed");
        exit(0);
    }
      
    // Populating server information
    servAddr = populateSocketAddrObject(serverIP, serverPort);
      
    // Bind the socket with the server address
    if (bind(sockfd, (const struct sockaddr *)&servAddr, sizeof(servAddr)) < 0)
    {
        perror("bind failed");
        exit(0);
    }    

    int clientMsgLen = sizeof(cliAddr);

    while(1)
    {
        memset(clientRequest,0,10);
        msgLen = recvfrom(sockfd, (char *)clientRequest, 10, 0, (struct sockaddr *) &cliAddr, &clientAddrLen);

        if(msgLen > 0)
        {
            k = fork();
            if(k==0)
            {
                if(isValidIP(cliAddr.sin_addr.s_addr)==false)
                {
		           fprintf(stdout, "Cannot accept request from the client. IP is not Allowed\n");
                   exit(0);
                }

                getClientSecretKeyandFileName(clientRequest, &clientSecretKey, fileName);

                if(clientSecretKey != secretKey)
                {
                    fprintf(stdout,"Secret key does not match for client and server.\n");
                    exit(0);
                }

                processClientRequest(fileName,&cliAddr, serverIP, &timeout, &windowSize, &blockSize);
                exit(0);
            }
        }
    }

    close(sockfd);
    return 0;
}