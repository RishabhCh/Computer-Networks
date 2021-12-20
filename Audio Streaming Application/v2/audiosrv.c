// UDP Server code.
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
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
#include "utility.h"


#define BUFSIZE 4096
#define MAX_FILENAME_LEN 8
#define MAX_BLOCKSIZE_LEN 2
#define MAX_BLOCK 4097
#define MAX_RETRANSMIT_ATTEMPTS 3
#define CLIENT_PACKET_SIZE 10
#define CLIENT_FEEDBACK_SIZE 2
#define PACKETS_AT_END 8

//int** buffer_log;
int buffer_log[100000][2];
int buffer_log_row = 0;

/** @brief This function checks if the client IP belongs to the known internal
 * 		   hosts IP or not. If the client IP does not match with the trusted
 *         IPs, then 0 is returned else 1 is returned.
 *
 * 	@param client_ip Pointer to the struct in_addr that holds the client IP.
 * 	@returns 1 is the IP is valid and trusted else 0.
 */
int is_client_ip_valid(struct in_addr* client_ip){

	// Print the IP of the client.
	fprintf(stdout, "IP address of the client is: %s\n", inet_ntoa(*client_ip));
	fflush(stdout);

	int is_ip_valid = 0;
	// Implement IP verification check.
	int fourth_byte_of_ip = (*client_ip).s_addr&0xFF;
	int thired_byte_of_ip = ((*client_ip).s_addr&0xFF00)>>8;
	int second_byte_of_ip = ((*client_ip).s_addr&0xFF0000)>>16;
	int first_byte_of_ip = ((*client_ip).s_addr&0xFF000000)>>24;

	if(fourth_byte_of_ip == 128 && thired_byte_of_ip == 10 && (second_byte_of_ip == 25 || second_byte_of_ip == 112))
		is_ip_valid = 1;
	else
		is_ip_valid = 0;

	return is_ip_valid;
}


/** @brief This function checks authenticates the client request by
 *		   performing various checks.
 *
 * 	It calls the is_client_ip_valid() function to verify if client IP
 *  is a known IP or not.
 *  Also checks the file requested by client exists or not.
 *
 * 	@param filename Name of the file requested by client.
 * 	@param cliaddr Pointer to struct sockaddr_in data type which stores
 *					client's address. 
 * 	@returns 1 if client is autheticated else 0.
 */
int authenticate_client_request(char* filename, struct sockaddr_in* cliaddr){

    int client_authenticated = 1;

    // Safety measure: Discard the client's request if client's IP is not from a known source.
	if(is_client_ip_valid(&(cliaddr->sin_addr))){
		fprintf(stdout, "IP Verification Successful. Client belongs to the list of known IPs.\n"); fflush(stdout);
	}
	else{
		fprintf(stdout, "IP Verification FAILED. Discarding the client's request!!\n"); fflush(stdout);
		client_authenticated = -1;
	}

	// Check if the requested audio file exists or not.
	if(access(filename, F_OK) != 0){
		fprintf(stdout, "File does not exist!! Hence, discarding the client request. %s\n", filename); fflush(stdout);
		client_authenticated = -1;
	}

	char ch;
	int filename_len = strlen(filename);
	if(filename_len > MAX_FILENAME_LEN){
		fprintf(stdout, "Filename length is greater than %d.\n", MAX_FILENAME_LEN); fflush(stdout);
		client_authenticated = -1;
	}
	
	if(filename[filename_len-3]!='.' && filename[filename_len-2]!='a' && filename[filename_len-1]!='u'){
		fprintf(stdout, "Filename %s does not end with `.au` extension.\n", filename); fflush(stdout);
		client_authenticated = -1;
	}

	if(client_authenticated == 1){
		for(int i=0; i<strlen(filename); i++){
			ch = filename[i];
			if (((ch>='a' && ch<='z') || (ch>='A' && ch<='Z') || ch=='.') && isascii(ch) == 0){
				client_authenticated = -1;
				break;
			}
		}
	}

	return client_authenticated;
}


/** @brief This function creates the UDP socket descriptor and bind 
 * 		   server's internet address and the port it is running on.
 *
 *	@param sockfd Pointer to the UDP connection socket descriptor.
 *  @param servaddr Pointer to struct sockaddr_in data type which stores
 * 					server's address, and port.
 *  @param ip  Pointer to the server IP.
 *  @param port Port on which the server is running.
 */
int create_socket_and_bind(int *sockfd, struct sockaddr_in* servaddr, char* ip, int port){

	// Creating the socket file descriptor for UDP using SOCK_DGRAM.
	if ((*sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		fprintf(stdout, "Socket creation failed for the child process, can't serve client's request\n"); fflush(stdout);
		return -1;
	}

    // // Build server's internet address and the port it is running on.
	bzero(servaddr, sizeof(*servaddr));
	servaddr->sin_family = AF_INET;
	servaddr->sin_addr.s_addr = inet_addr(ip);
	servaddr->sin_port = htons(port);
	
	// Bind the socket with the servaddr address.
	if (bind(*sockfd, (const struct sockaddr *)servaddr, sizeof(*servaddr)) < 0) {
		fprintf(stdout,"Failed to bind the server to the socket file descriptor.\n"); fflush(stdout);
		return -1;
	}
    
	return 1;
}

/** @brief This function writes the contents of the 2D array `buffer_log`
 *         to the logfile provided as input to this program.
 *
 *  @param logfile file to write logs to.
 */
void write_to_log_file(char* logfile){
	
	FILE *fp;

	if((fp=fopen(logfile, "wb"))==NULL)
   		do_error("Cannot open log file for writing.\n");

	for(int i=0;i<buffer_log_row;i++){
		fprintf(fp,"%u ",buffer_log[i][0]);
		fprintf(fp,"%u",buffer_log[i][1]);
		fprintf(fp,"\n");
	}

	fclose(fp);
}


/** @brief This function adds the normalized timestamp along with
 * 		   the current lambda value to the 2D array `buffer_log`.
 *
 * @param stream Pointer to struct streaming_config data type which stores
 *				 stream's configs.
 */  
void add_timestamp_and_lambda(streaming_config* stream){
	if(stream->pspacing == 0)
	return;
	if(stream->start_time == 0){
		stream->start_time = current_time_in_msec();
		buffer_log[buffer_log_row][0] = 0;
	}
	else{
		buffer_log[buffer_log_row][0] = (current_time_in_msec() - stream->start_time);
	}
	buffer_log[buffer_log_row][1] = (1000/stream->pspacing); // lambda is 1000/pspacing.
	buffer_log_row++;
}


/** @brief Reads the file from the local working directory and 
 * 		   sends it to the client.
 *
 *  The file is read in size of `server->blocksize` up until
 * 	the `server->windowsize` is filled.
 *
 *  @param sockfd The pointer to the UDP socket descriptor.
 * 	@param cliaddr Pointer to struct sockaddr_in data type which stores
 *				   client's address.
 * 	@param stream Pointer to struct streaming_config data type which stores
 *				  stream's configs.
 */
void send_file_to_client(int* sockfd, struct sockaddr_in* cliaddr, streaming_config* stream){

	FILE* fp;
	int num_bytes;
	char file_buffer[MAX_BLOCK];
	char client_feedback[CLIENT_FEEDBACK_SIZE];
    int message_length = 0;

    // Open the file to be read.
	fp = fopen(stream->filename, "rb");
	if (fp == NULL){
		fprintf(stdout, "Error in reading the file at server.\n"); fflush(stdout);
		return;
	}


    struct sockaddr_in s;
	int addr_length = sizeof(s);
	fprintf(stdout, "Sending file %s to the client number: %d...\n", stream->filename, stream->client_number); fflush(stdout);
	stream->start_time = 0;
	int sleep_counter = 0;

	struct timespec remaining, request = {0, (stream->pspacing)*1000000};

    memset(file_buffer, 0, sizeof(file_buffer));
	memset(client_feedback, 0, CLIENT_FEEDBACK_SIZE);
	long long val = 0;
	while((num_bytes = fread(file_buffer, 1, stream->blocksize, fp)) > 0) {
		// Send the data packet to client
		sendto(*sockfd, (const char *)file_buffer, num_bytes, 0, 
				(const struct sockaddr *) cliaddr, addr_length);			
		add_timestamp_and_lambda(stream);
    
		nanosleep(&request, &remaining);

		// Non-blocking recvfrom() to get the feedback from client.
		while((message_length = recvfrom(*sockfd, (char *)&client_feedback, CLIENT_FEEDBACK_SIZE, 
								MSG_DONTWAIT, (struct sockaddr *)cliaddr, &addr_length)) > 0){
			
			memcpy(&(stream->pspacing), client_feedback, CLIENT_FEEDBACK_SIZE);
			fprintf(stdout, "pspacing received from client %d with value: %u\n", stream->client_number, stream->pspacing); fflush(stdout);
		
			val = (stream->pspacing)*1000000;
			request.tv_nsec = MIN(999999999, val);
		}

		
		memset(file_buffer, 0, sizeof(file_buffer));
		
	}
	fclose(fp);

    // Send 8 UDP packets with empty payload to the client to indicate end of transmission.
	for(int i=0; i<PACKETS_AT_END; i++){
		sendto(*sockfd, (const char *)file_buffer, 0, 0, 
				(const struct sockaddr *) cliaddr, addr_length);
	}
    write_to_log_file(stream->logfile);
	
}


/** @brief Handle the incoming client request by sending the requested file
 *         if the client is authenticated.
 *
 * 	@param server Pointer to struct config data type which stores
 *				  server's configs.
 * 	@param cliaddr Pointer to struct sockaddr_in data type which stores
 *				   client's address.
 * 	@param client_request Buffer that contains the client request.
 *  @param client_number Number associated with each client to distinguish between
 *                       different clients.
 */
void handle_client_request(srv_config* server, struct sockaddr_in *cliaddr, char* client_request, int client_number){
	
	char filename[8];
	char *str;
	int sockfd, length;
	unsigned short blocksize;
	struct sockaddr_in servaddr;

    memcpy(&blocksize, client_request, MAX_BLOCKSIZE_LEN);
	memset(filename, 0, MAX_FILENAME_LEN);
	memcpy(filename, &client_request[MAX_BLOCKSIZE_LEN], MAX_FILENAME_LEN);
	
	
	// Authenticate the client request, return if client is not authenticated.
	if(authenticate_client_request(filename, cliaddr) == -1) return;

	fprintf(stdout, "Client %d is successfully autheticated.\n", client_number); fflush(stdout);
	
    // Create a new socket to serve this particular client's request.
	if(create_socket_and_bind(&sockfd, &servaddr, server->ip, 0) == -1){
		fprintf(stdout, "Cannot create and bind socket to address client number %d's request.\n", client_number); 
		return;
	}
	else{
		streaming_config stream;
		strcpy(stream.filename, filename);
		stream.client_number = client_number;
	    stream.blocksize = blocksize;
		stream.pspacing = 1000/(server->lambda); // packets per milliseconds.
		strcpy(stream.logfile, server->logfile);
		length = snprintf(NULL, 0, "%d", client_number);
		str = malloc(length + 1);
		snprintf(str, length + 1, "%d", client_number);
		strcat(stream.logfile, str);
		free(str);
		fprintf(stdout, "Proceeding with file transfer to client number %d\n", client_number); 
		send_file_to_client(&sockfd, cliaddr, &stream);
	}
}


int main(int argc, char **argv) {

	/* Check if 7 command line arguments are received or not, which are program
	 * name, server IP, server PORT, secret key, blocksize, windowsize and timeout.*/
	if(argc!=5)
		do_error("Please provide server's IP, Port, lambda and the log file name.\n");

	// User input to this server program.
	srv_config server;
	server.ip = argv[1]; // server IP is stored as a string
	server.port = strtol(argv[2], NULL, 10); // server port is stored as an integer.
	server.lambda = strtol(argv[3], NULL, 10); // initial sending rate in unit of packets per second.
	server.logfile = argv[4]; // logfile name is stored as a string.

    // Define the variables to be used in this program.
    unsigned char client_request[BUFSIZE]; // Buffer to store the client's request.

	int sockfd; // file descriptor to be used by sockets.
	int addr_length; // holds the size of the struct sockaddr_in cliaddr defined below.
	int message_length; // length of the message received from client.
	
	struct sockaddr_in servaddr, cliaddr;
	addr_length = sizeof(cliaddr);
	pid_t childpid; // stores the process id of the child created from fork().

	if(create_socket_and_bind(&sockfd, &servaddr, server.ip, server.port) == -1)
		do_error("Failed to create and bind socket to server's address.\n");

	/* Run a while loop to get client message using recvfrom(). Perform client IP 
	 * validation check as a security measure and call handle_client_request() to 
	 * service the client.
     */
	int client_number = 0;
	while (1) {
		memset(client_request, 0, BUFSIZE);
		memset(&cliaddr, 0, sizeof(cliaddr));
	    message_length = recvfrom(sockfd, (char *)client_request, BUFSIZE, 
								0, (struct sockaddr *) &cliaddr, &addr_length);

		if (message_length >= CLIENT_PACKET_SIZE){
			client_number++;
			// Create a child process to handle the client request.
			if ((childpid = fork()) == 0){
				handle_client_request(&server, &cliaddr, client_request, client_number);
				exit(0);
			}
		}
	}
	close(sockfd);
	return 0;
}
