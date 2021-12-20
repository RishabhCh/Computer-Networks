// FTP Client
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
#include <ctype.h>

#define NUMBEROFARGS 6 // Number of command line arguments allowed
#define FILENAMESIZE 8 // Maximum number of characters allowed for a filename

double filesize = 0; // To store the file size

void alarm_handler(int sig)
{
    // No action required here when alarm goes off
}

void verifyNumberOfArgs(int numberOfArguments)
{
	// Terminate execution if incorrect number of arguments are provided
	if(numberOfArguments != NUMBEROFARGS)
	{
		fflush(stdout);
		fprintf(stdout,"Incorrect number of command line arguments provided.\nThe FTP client needs 5 command line arguments namely - Server IP, Server Port, Filename, Secret-Key, Blocksize.\n");
		exit(0);
	}
}

void verifyCommandLineArguments(char filename[], int secretKey)
{

	// Terminate execution if file name is greater than 8 characters
	if(strlen(filename) > FILENAMESIZE)
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
	if(secretKey<0 || secretKey>65535)
	{
		fflush(stdout);
		fprintf(stdout,"Secret key is outside the range of [0,65535]. Please try with a valid secret key.\n");
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

void createAndConnectSocket(int *serverSocketDescriptor, struct sockaddr_in *servAddr)
{
	// Creating the socket
	if ((*serverSocketDescriptor = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
  	{
        fflush(stdout);
        fprintf(stdout,"Unable to create socket.\n");
        exit(0);
    }

  	// Connect to server
  	if (connect(*serverSocketDescriptor, (struct sockaddr*)servAddr, sizeof(*servAddr)) < 0) 
    {
        fflush(stdout);
        fprintf(stdout,"Unable to connect.\n");
        exit(0);
    }
}

// Function to record the timestamp
double get_time_in_msec()
{
	struct timeval ctime;
	gettimeofday(&ctime,NULL);
	return ((ctime.tv_sec*1000)+(ctime.tv_usec/1000));
}

void readFileFromServer(int *serverSocketDescriptor, struct sockaddr_in *servAddr,int blockSize, char clientRequest[], double *requestSendTime, double*fileReceiveTime, char filename[])
{
	int connectionRetryCount = 1; // Count of the number of times the request has been sent to the server
	int isFirstTimeRead = 0; // Indicator for checking if the first block of file has arrived from the server
	char fileBlock[blockSize+10]; // Block of file
	FILE *filePointer; // File Pointer
	filesize = 0;

	while(1){
	// Interrupt for read command. If read does not get any value till 2 seconds, this will interrupt its execution.
	siginterrupt(SIGALRM, 1);
	// Signal Handler
	signal(SIGALRM, alarm_handler);
	// Alarm for 2 seconds
	alarm(2);

	// Read server response
	ssize_t msgLen = read(*serverSocketDescriptor, fileBlock, blockSize);
	if(msgLen < 0)
	{
		if(errno != EINTR)
		{
			fflush(stdout);
			fprintf(stdout,"Cannot Read Response.\n");
			break;
		}
		else
		{
			fprintf(stdout,"Request Rejected By the Server.\n");

			// Alarm Expired. Need to reconnect depending in the number of retries.
			close(*serverSocketDescriptor);
			
			if(connectionRetryCount < 3)
			{
				fprintf(stdout,"Trying to Resend the request...\n");
    			// Create and Connect to Socket
				createAndConnectSocket(serverSocketDescriptor, servAddr);

				// Get timestamp before transmitting the request to the server
				*requestSendTime = get_time_in_msec();

				// Writing the message to the server
				if (write(*serverSocketDescriptor, clientRequest, 10) < 0)
   				{ 
					fprintf(stdout,"Error is sending message to the Server\n");
					exit(0);
				}	
					
				connectionRetryCount++;
					
				alarm(2);				
			}
			else
			{
				fprintf(stdout,"The Client could not connect for 3 times. Terminating Client.\n");
				exit(0);
			}
		}
	}
	else if(msgLen > 0)
	{
		filesize = filesize + msgLen;
		alarm(0);
		if(isFirstTimeRead == 0)
		{
			isFirstTimeRead = 1;
			char fName[30] = "ReceivedFile_";
			strcat(fName,filename);
			filePointer = fopen(fName,"wb");
		}

		fwrite(&fileBlock,msgLen,1,filePointer);
		bzero(fileBlock,strlen(fileBlock));
		fsync(*serverSocketDescriptor);
	}
	else if(msgLen==0)
	{
		alarm(0);
		*fileReceiveTime = get_time_in_msec();
		fprintf(stdout,"File has been read.");
		fclose(filePointer);
		break;
	}
	}
}

// This is the function from where the execution will start
// argc will store the number of command line arguments
// argv is an array that will store the values of command line arguments
int main(int argc, char **argv) 
{

	verifyNumberOfArgs(argc);

	int serverSocketDescriptor; // To store the socket descriptor
	double requestSendTime = 0, fileReceiveTime = 0; // To store the timestamp before transmitting the client request
	struct sockaddr_in servAddr; // To store the socket details for server
	unsigned char clientRequest[10];
	int connectionRetryCount = 1;
	int blocksize = atoi(argv[5]);
	char *filename = argv[3];

	printf("BlockSize %d\n",blocksize);

	// Calling the function to validate number of arguments, file name and secret key
	verifyCommandLineArguments(argv[3],atoi(argv[4]));

	// Populate Server Address Object
	servAddr = populateSocketAddrObject(argv[1],atoi(argv[2]));

	// Create and Connect to Socket
	createAndConnectSocket(&serverSocketDescriptor, &servAddr);    

	// Get timestamp before transmitting the request to the server
	requestSendTime = get_time_in_msec();

	// Making the client message secret key and file name
	unsigned short secretKey = strtoul(argv[4],NULL,10);
	memset(clientRequest,0,sizeof(clientRequest));
	memcpy(clientRequest,&secretKey,2);
	memcpy(&clientRequest[2],filename,8);

	// Writing the message to the server
	if (write(serverSocketDescriptor, clientRequest, 10) < 0)
    { 
		fprintf(stdout,"Error is sending message to the Server\n");
		exit(0);
	}

	readFileFromServer(&serverSocketDescriptor, &servAddr, blocksize, clientRequest, &requestSendTime, &fileReceiveTime, filename);

	fprintf(stdout,"File Size is: %f bytes\n",filesize);
	fprintf(stdout,"Round Trip Time is: %f milliseconds\n",(fileReceiveTime - requestSendTime));
	fprintf(stdout,"Throughput is: %f Megabits / Seconds\n",(filesize/(fileReceiveTime - requestSendTime))*0.008);

    // Close Socket
    close(serverSocketDescriptor);

    return 0;
}
