// Client code.
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
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
		fprintf(stdout,"Incorrect number of command line arguments provided.\nThe Client needs 6 command line arguments namely - Server IP, Server Port, Filename, Secret-Key, Blocksize, windowSize.\n");
		exit(0);
	}
}

void verifyCommandLineArguments(char filename[], unsigned int *secretKey, int *blockSize)
{

	// Terminate execution if file name is greater than 8 characters
	if(strlen(filename) > 8)
	{
		fflush(stdout);
		fprintf(stdout,"File name needs to be changed. The length of filename exceeds 8 characters.\n");
		exit(0);
	}

	// Terminate execution if the file name contains any character other than lowecase and uppercase letters
	for(int i=0;filename[i]!='\0';i++)
	{
		if(filename[i] != '.' && isalpha(filename[i])==0)
		{
			fflush(stdout);
			fprintf(stdout,"The filename is not valid because it contains non-alphabetic characters.\n");
			exit(0);
		}
	}

	// Terminate if the secret key falls outside the permitted range [0,65535]
	if((*secretKey<0) || (*secretKey>65535))
	{
		fflush(stdout);
		fprintf(stdout,"Secret key is outside the range of [0,65535]. Please try with a valid secret key.\n");
		exit(0);
	}

    if((*blockSize)>1471)
    {
        fflush(stdout);
		fprintf(stdout,"BlockSize > 1471 is not allowed.\n");
		exit(0);
    }
}

struct sockaddr_in populateSocketAddrObject(char *ipAddress,int portNumber)
{
    struct sockaddr_in socketAddr;

    // Initialize the object with zeroes
    memset(&socketAddr, 0, sizeof(socketAddr));

    // Populate the object
	socketAddr.sin_family = AF_INET; // IPv4
	socketAddr.sin_addr.s_addr = inet_addr(ipAddress); // IP Address
	socketAddr.sin_port = htons(portNumber); // Port

    return socketAddr;
}

// Form the client request from secret key and port number
void constructClientRequest(unsigned char clientRequest[], unsigned int *secretKey, char *fileName)
{
	memset(clientRequest,0,sizeof(clientRequest));
	memcpy(clientRequest,secretKey,4);
	memcpy(&clientRequest[4],fileName,8); 

    fprintf(stdout,"XOR Secret Key to be sent to server %u.\n",(*secretKey));
    fprintf(stdout,"File Name to be sent to server %s.\n",fileName);
}

double getTime()
{
	struct timeval ctime;
	gettimeofday(&ctime,NULL);
	return ((ctime.tv_sec*1000)+(ctime.tv_usec/1000));
}

void processServerResponse(int *sockfd, char*fileName, int *windowSize, int *blockSize, double *fileSize)
{
    *fileSize = 0;
    int msgLen = 0, bufferSize = 0, ctr = 0, totalPackets = 0, ackNos = 1;
    char ack;
    struct sockaddr_in servAddr;
    struct itimerval timer;
    int bufferIndex;

    char serverResponse[2048];
    char windowBuffer[64][1471];
    memset(windowBuffer,0,sizeof(windowBuffer));

    int addLen = sizeof(servAddr);
    bool eof = false;

    FILE *filePointer;
    char fName[30] = "ReceivedFile_";
    strcat(fName,fileName);

    filePointer = fopen(fName,"wb");

    while(1)
    {
        memset(serverResponse,0,2048);
        msgLen = recvfrom(*sockfd, (char *)serverResponse, 2048, 0, (struct sockaddr *)&servAddr,&addLen);
        
        if(msgLen > 0)
        {
            timer.it_value.tv_sec =  0;
	        timer.it_value.tv_usec = 0;   
	        timer.it_interval = timer.it_value;
	        setitimer(ITIMER_REAL, &timer, NULL);

            ctr = (int)serverResponse[0];
            
            bufferIndex = ctr%(*windowSize);

            if(windowBuffer[bufferIndex][0]==0)
            {
                if(msgLen < (*blockSize)+1)
                {
                    eof = true;
                }
                if(msgLen == (*blockSize) + 2)
                {
                    eof = true;
                    msgLen = msgLen - 1;
                }
                memcpy(&windowBuffer[bufferIndex][0], &serverResponse[1], msgLen - 1);
                *fileSize = (*fileSize) + (msgLen - 1);
                totalPackets += 1;
            }
        }
        else if(msgLen < 0)
        {
            return;
        }
        else if(errno == EINTR)
        {
            return;
        }

        if((totalPackets%(*windowSize)) == 0 || eof == true)
        {
            // write into the file
            for(int i=0; i<=((totalPackets-1)%(*windowSize));i++)
            {
                if(i==((totalPackets-1)%(*windowSize)))
                {
                    fwrite(&windowBuffer[i][0],1,msgLen-1,filePointer);
                }
                else
                {
                    fwrite(&windowBuffer[i][0],1,(*blockSize),filePointer);
                }
            }
            ack = (char)(totalPackets-1);

            if(eof == false)
            {
                fprintf(stdout,"Packets in the window have been received and written.\n");
                fprintf(stdout,"Sending Acknowledgement Message: %d.\n",(totalPackets-1));
                sendto(*sockfd, (const char *)&ack,1,0,(const struct sockaddr*)&servAddr,addLen);
                if(totalPackets == (*windowSize)*2) 
                {
                    totalPackets = 0;
                }
            }

            if(eof == true)
            {
                fprintf(stdout,"Sending acknowledgements for end of file.\n");
                // Send 8 packets at the end of file
                for(int i=0; i<8;i++)
                {
                    fprintf(stdout,"Sending Acknowledgement Message (for end of file): %d.\n",(totalPackets-1));
                    sendto(*sockfd, (const char *)&ack,1,0,(const struct sockaddr*)&servAddr,addLen);                    
                }
                break;
            }
            memset(windowBuffer,0,sizeof(windowBuffer));
        }        
    }
    fclose(filePointer);
}

unsigned int bbdecode(unsigned int x, unsigned int prikey)
{
    fprintf(stdout,"Decoding:\n");
    fprintf(stdout,"Client IP in int: %u",x);
    fprintf(stdout,"Client Primary Key in int: %u",prikey);

    unsigned int decodedKey;
    decodedKey = x^prikey; // XOR

    return decodedKey;
}

char * getClientIP()
{
    char hostbuffer[256];
    char * clientIP;
    struct hostent *host_entry;
    int hostname;
  
    // To retrieve hostname
    hostname = gethostname(hostbuffer, sizeof(hostbuffer));
  
    // To retrieve host information
    host_entry = gethostbyname(hostbuffer);
  
    // To convert an Internet network
    // address into ASCII string
    clientIP = inet_ntoa(*((struct in_addr*) host_entry->h_addr_list[0])); 
    return clientIP;
}

// Driver code
int main(int argc, char **argv) 
{
    // Verify that correct number of command line arguments are provided
    verifyNumberOfArgs(argc);

    // Initializing Parameters
    char *serverIP = argv[1];
    int serverPort = atoi(argv[2]);
    char *fileName = argv[3];
    int blockSize  = atoi(argv[5]);
    int windowSize = atoi(argv[6]);
    unsigned int secretKey = strtoul(argv[4],NULL,10);
    unsigned char clientRequest[20];
    int sockfd, msgLen, addLen;
    struct sockaddr_in servAddr;
    double startTime, endTime, fileSize;
    struct itimerval timer;
    unsigned int encodedSecretKey;
    char *clientIP;

	// Calling the function to validate number of arguments, file name and secret key
	verifyCommandLineArguments(fileName,&secretKey,&blockSize);

    fprintf(stdout,"Command Line Parameters: \n");
    fprintf(stdout,"Server IP Address: %s\n",serverIP);
    fprintf(stdout,"Server Port      : %d\n",serverPort);
    fprintf(stdout,"File Name        : %s\n",fileName);
    fprintf(stdout,"Secret Key       : %d\n",secretKey);
    fprintf(stdout,"Block Size       : %d\n",blockSize);
    fprintf(stdout,"Window Size      : %d\n",windowSize);    

    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
    {
        perror("socket creation failed");
        exit(0);
    }

    // Populating server information
    servAddr = populateSocketAddrObject(serverIP,serverPort);

    // Get IP Address of the Client
    clientIP = getClientIP();

    printf("Client IP is: %s\n", clientIP);  

    encodedSecretKey = bbdecode(inet_addr(clientIP),secretKey);

	// Making the client message from secret key and file name
    constructClientRequest(clientRequest, &encodedSecretKey, fileName);
    
    addLen = sizeof(servAddr);

    int attempts = 0;

    while(attempts++ < 5)
    {
        startTime = getTime();
    
        // Set timer for 500 ms
	    siginterrupt(SIGALRM, 1);
	    signal(SIGALRM, alarm_handler);
	    timer.it_value.tv_sec =  0.5;
	    timer.it_value.tv_usec = 500000;   
	    timer.it_interval = timer.it_value;
	    setitimer(ITIMER_REAL, &timer, NULL);

        sendto(sockfd, (const char*)clientRequest,12,0,(const struct sockaddr *) &servAddr, addLen);
        fflush(stdout);
        fprintf(stdout,"Request Sent to the server.\n");
        processServerResponse(&sockfd, fileName, &windowSize, &blockSize, &fileSize);

        if(fileSize > 0)
        {
	        timer.it_value.tv_sec = 0;
	        timer.it_value.tv_usec = 0;   
	        timer.it_interval = timer.it_value;
	        setitimer(ITIMER_REAL, &timer, NULL);            

            endTime = getTime();

            fprintf(stdout,"File Successfully Transferred.\n");
        
        	fprintf(stdout,"File Size is: %f bytes\n",fileSize);
	        fprintf(stdout,"Round Trip Time is: %f milliseconds\n",(endTime - startTime));
	        fprintf(stdout,"Throughput is: %f Megabits / Seconds\n",(fileSize/(endTime - startTime))*0.008);            
            break;
        }
    }
    close(sockfd);

    if(attempts >= 5)
    {
        fprintf(stdout,"Could not receive server response for 5 attempts. Terminating Client\n");
    }

    return 0;
}