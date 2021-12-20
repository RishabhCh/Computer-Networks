// UDP Client code.
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>
#include <time.h>
#include <semaphore.h>
#include <sys/types.h> 
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <alsa/asoundlib.h>
#include "utility.h"

#define BUFSIZE 4096
#define AUDIO_BLOCK 4096
#define MAX_FILENAME_LEN 8
#define MAX_BLOCKSIZE_LEN 2
#define MAX_BYTES_TO_SERVER 10
#define MAX_REQ_ATTEMPTS 3
#define SERVER_RESPONSE_TIME 1000 // This is in milliseconds.
#define AUDIO_PLAY_INTERVAL 313 // This is in milliseconds.



int stream_audio = 0;
sem_t mutex;
cli_config client;
char* response_buffer;
char* audio_buffer;
//int** buffer_log;
int buffer_log[100000][2];
int buffer_log_row = 0;
struct sockaddr_in servaddr;
int sockfd;

// constants for control law
double epsilon = 0, beta = 0;

static snd_pcm_t *mulawdev;
static snd_pcm_uframes_t mulawfrms;

// Initialize audio device.
void mulawopen(size_t *bufsiz) {
	snd_pcm_hw_params_t *p;
	unsigned int rate = 8000;

	snd_pcm_open(&mulawdev, "default", SND_PCM_STREAM_PLAYBACK, 0);
	snd_pcm_hw_params_alloca(&p);
	snd_pcm_hw_params_any(mulawdev, p);
	snd_pcm_hw_params_set_access(mulawdev, p, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(mulawdev, p, SND_PCM_FORMAT_MU_LAW);
	snd_pcm_hw_params_set_channels(mulawdev, p, 1);
	snd_pcm_hw_params_set_rate_near(mulawdev, p, &rate, 0);
	snd_pcm_hw_params(mulawdev, p);
	snd_pcm_hw_params_get_period_size(p, &mulawfrms, 0);
	*bufsiz = (size_t)mulawfrms;
	return;
}

// Write to audio device.
#define mulawwrite(x) snd_pcm_writei(mulawdev, x, mulawfrms)

// Close audio device.
void mulawclose(void) {
	snd_pcm_drain(mulawdev);
	snd_pcm_close(mulawdev);
}


void assignControlConstants()
{
   FILE *fptr;

   // open audiocliparam file
   fptr = fopen("audiocliparam.dat","r");
   if(fptr == NULL)
   {
		fprintf(stdout,"Error in opening the audiocliparam file\n");
		exit(1);
   }
   
   // reading values for N, T, D and S
   fscanf(fptr,"%lf",&epsilon);
   fprintf(stdout,"Epsilon: %lf\n",epsilon);
   if(client.method == 1)
   {
	   fscanf(fptr,"%lf",&beta);
	   fprintf(stdout,"Beta: %lf\n",beta);
   }

   fclose(fptr);
   
}

/** @brief Validates the user input provided as command
 *         line arguments.
 *
 *  Validate that the filename if of 8 bytes at max and 
 *  contains valid ASCII character, and only lower-case 
 *	and upper-case alphabets or a '.' used for file extension.
 *  Also validates that the buffersize and targetbuf are multiples
 *  of 4096.
 *
 *  @param client Pointer to struct cli_config that stores client's configs.
 *  @returns 1 if input is valid, else 0.
 */
int validate_input(cli_config* client){
	int is_input_valid = 1;
	char ch;
	int filename_len = strlen(client->filename);
	if(filename_len > MAX_FILENAME_LEN)
		is_input_valid = 0;
	
	if(client->filename[filename_len-3]!='.' && client->filename[filename_len-3]!='a' && client->filename[filename_len-3]!='u')
		is_input_valid = 0;

	if(is_input_valid){
		for(int i=0; i<strlen(client->filename); i++){
			ch = client->filename[i];
			if (((ch>='a' && ch<='z') || (ch>='A' && ch<='Z') || ch=='.') && isascii(ch) == 0){
				is_input_valid = 0;
				break;
			}
		}
	}

	if(client->buffersize%BUFSIZE != 0 || client->targetbuf%BUFSIZE != 0)
		is_input_valid = 0;
	
	return is_input_valid;
}


/** @brief This function creates the UDP socket descriptor and bind 
 * 		   server's internet address and the port it is running on.
 *
 *	@param sockfd Pointer to the UDP connection socket descriptor.
 *  @param servaddr Pointer to struct sockaddr_in data type which stores
 * 					server's address, and port.
 *  @param server_ip  Pointer to the server IP.
 *  @param server_port Port on which the server is running.
 */
void create_socket_and_bind(int* sockfd, struct sockaddr_in* servaddr,
							char* server_ip, int server_port){

  	// Creating the socket file descriptor for UDP using SOCK_DGRAM.
	if ((*sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
  		do_error("Socket creation failed for the client.\n");

	// Build server's internet address and the port it is running on.
    bzero((char *) servaddr, sizeof(*servaddr));
    servaddr->sin_family = AF_INET;
    servaddr->sin_addr.s_addr = inet_addr(server_ip);
    servaddr->sin_port = htons(server_port);
}


/** @brief This function sends the feedpacket packet to the server
 *         with the new pspacing value.
 *
 *  When the lamda values falls down below 0, then a max wait time of 999 milliseconds
 *  is sent to the server. 
 */
void send_feedback_to_server(){

    char feedback[2];
	unsigned short pspacing;
	int addr_length  = sizeof(servaddr);
	if(client.lambda <= 0){
		pspacing = 999;
	}
	else{
		pspacing = (1000/client.lambda); // in milliseconds
	}
	client.pspacing = pspacing;
	memset(feedback, 0, 2);
	memcpy(feedback, &pspacing, 2);
	
	sendto(sockfd, (const char *)feedback, 2, 
			0, (const struct sockaddr *) &servaddr, 
			addr_length);
}

/** @brief This function writes the contents of the 2D array `buffer_log`
 *         to the logfile provided as input to this program.
 */
void write_to_log_file(){
	
	FILE *fp;

	if((fp=fopen(client.logfile, "wb"))==NULL)
   		do_error("Cannot open log file for writing.\n");

    for(int i=0;i<buffer_log_row;i++){
		fprintf(fp,"%u ",buffer_log[i][0]);
		fprintf(fp,"%u",buffer_log[i][1]);
		fprintf(fp,"\n");
	}
	fclose(fp);
}


/** @brief This function adds the normalized timestamp along with
 * 		   the current buffersize to the 2D array `buffer_log`.
 */  
void add_timestamp_and_bufsize(){
	if(client.start_time == 0){
		client.start_time = current_time_in_msec();
		buffer_log[buffer_log_row][0] = 0;
	}
	else{
		buffer_log[buffer_log_row][0] = (current_time_in_msec() - client.start_time);
	}
	buffer_log[buffer_log_row][1] = client.current_bufsize;
	buffer_log_row++;
}

void updateLambda()
{
	unsigned short lambda = 0;
	unsigned short gamma = (1000*4096)/313;
	lambda = (client.lambda)*client.blocksize + epsilon * ((client.targetbuf - client.current_bufsize)*1.0);
	if(client.method == 1)
	{
		lambda = lambda - beta * ((lambda - gamma)*1.0);
	}

	client.lambda = lambda/client.blocksize;
	fprintf(stdout,"-----Updated Lambda----: %u\n",client.lambda);
}

/** @brief This function writes data to the buffer in a round robin fashion.
 *  
 *  It calls the add_timestamp_and_bufsize() in the end to log the 
 *  current buffersize along with the timestamp.
 */
void write_to_buffer(){
	int bytes_to_copy = 0;
	sem_wait(&mutex);
	if((client.producer_pointer + client.last_message_length) > client.buffersize){
		bytes_to_copy = (client.buffersize - client.producer_pointer);
		memcpy(&audio_buffer[client.producer_pointer], &response_buffer[0], bytes_to_copy);
		client.producer_pointer = 0;		
	}
	memcpy(&audio_buffer[client.producer_pointer], &response_buffer[bytes_to_copy], (client.last_message_length-bytes_to_copy));
	client.producer_pointer += (client.last_message_length-bytes_to_copy);
	client.current_bufsize += client.last_message_length;
	updateLambda();
	send_feedback_to_server();
	fprintf(stdout, "pspacing value sent to server on write: %u\n", client.pspacing); fflush(stdout);
	add_timestamp_and_bufsize();
	sem_post(&mutex);
}


/** @brief This function reads bytes from the buffer to stream audio
 * 		   through mulawwrite() function call.
 *  
 *  It calls the add_timestamp_and_bufsize() in the end to log the 
 *  current buffersize along with the timestamp.
 */
void read_from_buffer(){
	stream_audio = 0;
	if(client.current_bufsize >= AUDIO_BLOCK){
		char* buf; 
		buf = malloc(AUDIO_BLOCK);
		memset(buf, 0, AUDIO_BLOCK);
		sem_wait(&mutex);
		memcpy(buf, &audio_buffer[client.consumer_pointer], AUDIO_BLOCK);
		mulawwrite(buf);
		set_timer(AUDIO_PLAY_INTERVAL);
		client.consumer_pointer = (client.consumer_pointer + AUDIO_BLOCK) % client.buffersize;
		client.current_bufsize -= AUDIO_BLOCK;
		free(buf);
		updateLambda();
		send_feedback_to_server();
		fprintf(stdout, "pspacing value sent to server on read: %u\n", client.pspacing); fflush(stdout);
		add_timestamp_and_bufsize();
		sem_post(&mutex);
	}
	else{
		set_timer(AUDIO_PLAY_INTERVAL);
	}
}


/** @brief Signal handler function that handles the interrupt caused
 *         by `SIGALRM`.
 *
 *  It calls the read_from_buffer() to read bytes from the buffer and 
 *  stream audio.
 *
 *	@param sig Code that represents the generated interrupt.
 */
void signal_handler(int sig){
	stream_audio = 1;
}

/** @brief Receive audio file contents from the server, store it in buffer
 *		   and stream.
 *
 *  @param sockfd The pointer to the UDP socket descriptor.
 *  @returns -1 for an error else 0
 */
int get_server_response_to_stream(int* sockfd){

	response_buffer = malloc(BUFSIZE);
	 
	audio_buffer = malloc(client.buffersize);
	memset(audio_buffer, 0, client.buffersize);

	char feedback[2];
	unsigned short pspacing = 0;

	client.audio_block = AUDIO_BLOCK;
	client.current_bufsize = 0;
	client.producer_pointer = 0;
	client.consumer_pointer = 0;
	client.last_message_length = 0;
	client.start_time = 0;
    
    int addr_length = sizeof(servaddr);
	int packet_number = 0;
	int error = 0;
	int last_packet_rcvd = 0;

    fprintf(stdout, "Waiting for server to respond..\n"); fflush(stdout);
	mulawopen(&client.audio_block);
	siginterrupt(SIGALRM, 1);
	signal(SIGALRM, signal_handler);
	sem_init(&mutex, 0, 1);
	int timer_started = 0;
	while(1){
	    // Waiting on server's response to get the packets containing file data.
		memset(response_buffer, 0, BUFSIZE);
		client.last_message_length = -2;
		client.last_message_length = recvfrom(*sockfd, (char *)response_buffer, BUFSIZE, 
        							0, (struct sockaddr *)&servaddr, &addr_length);

							

		fprintf(stdout, "Message received from server of length: %d\n", client.last_message_length);
		
		if (client.last_message_length == -1 && errno == EINTR && packet_number == 0) { 
			error = 1; 
			break; 
		}

        // This indicates that the server has already transmitted the last packet.
		if(client.last_message_length == 0) last_packet_rcvd = 1;
		if(last_packet_rcvd == 1 && client.current_bufsize < AUDIO_BLOCK) {
			set_timer(0);
			break;
		}
		
	    if(client.last_message_length > 0){
			if(packet_number == 0) set_timer(0);
		    packet_number++;
			write_to_buffer();
			if(client.current_bufsize >= client.targetbuf && timer_started == 0){ set_timer(AUDIO_PLAY_INTERVAL), timer_started = 1;}
		}

		if(stream_audio == 1){
			packet_number++;
			read_from_buffer();
		}
	}
    mulawclose();
	free(audio_buffer);
	free(response_buffer);
	sem_destroy(&mutex);
	if(error) return -1;
	return 0;
}

int main(int argc, char **argv) {

	/* Check if 7 command line arguments are received or not, which are 
	 * program name, server IP, server PORT, filename, secret key, blocksize and windowsize.*/
	if(argc!=10)
		do_error("Please provide the required inputs to the client.\n");
	
    // Define the variables to be used in this program.
    client.server_ip = argv[1]; // server IP is stored as a string
    client.server_port = strtol(argv[2], NULL, 10); // server port is stored as an integer
    client.filename = argv[3]; // filename to be sent to the server.
	client.blocksize = strtol(argv[4], NULL, 10); // blocksize to read bytes used for File I/O.
	client.buffersize = strtol(argv[5], NULL, 10); // buffersize at the client.
	client.targetbuf = strtol(argv[6], NULL, 10); // buffersize aiming to be achieved.
	client.lambda = strtol(argv[7], NULL, 10); // initial sending rate in unit of packets per second.
	client.method = strtol(argv[8], NULL, 10); // congestion control method to be used.
	client.logfile = argv[9]; // logfile name is stored as a string.	

	// Check if input meets the specifications or not.
	if(!validate_input(&client))
		do_error("Aborting program execution as the inputs are invalid.\n");

	// assign epsilon and beta
	assignControlConstants();

    //int sockfd; // file descriptor to be used by socket
    ssize_t message_length; // length of the message received from server.
	unsigned char user_input[MAX_BYTES_TO_SERVER];
    char buffer[BUFSIZE]; // Buffer to store the server's response.
    struct sockaddr_in serveraddr; // Holds the server's address.
    int number_of_attempts = 0; // number of attempts made by client to send a packet to server.
    int addr_length = sizeof(serveraddr);
	unsigned short pspacing;

	// Create a socket and bind the address to it.
	create_socket_and_bind(&sockfd, &serveraddr, client.server_ip, client.server_port);

    // Create the client request packet sent to server.
	memset(user_input, 0, sizeof(user_input));
	memcpy(user_input, &client.blocksize, MAX_BLOCKSIZE_LEN);
	memcpy(&user_input[2], client.filename, MAX_FILENAME_LEN);

    int response;
	while(number_of_attempts++ < MAX_REQ_ATTEMPTS){
		// Set the timer.
		set_timer(SERVER_RESPONSE_TIME);

		// Send user input to the server.
		sendto(sockfd, (const char *)user_input, MAX_BYTES_TO_SERVER, 
        		0, (const struct sockaddr *) &serveraddr, 
        		addr_length);
		
		fprintf(stdout, "Request sent to the server. Attempt number: %d\n", number_of_attempts); fflush(stdout);
	    response = get_server_response_to_stream(&sockfd);
		if(response == -1){
			/* The program reaches here when timer expires while 
			 * no response is received from the server. 
			 * A new connection is established and packet is retransmitted. 
			 */
			close(sockfd);
			// Creating the socket file descriptor for UDP using SOCK_DGRAM.
			if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
  				do_error("Socket creation failed for the client.\n");
			continue;
		}
		else{
			// Output the metrics like throughput to stdout.
			fprintf(stdout, "Audio streaming done!! Writing the changing buffersize values with timestamps to log file: %s\n", client.logfile);
			fflush(stdout);
			write_to_log_file();
			break;
		}
	}
	close(sockfd);
    
	// Number of attempts greate than MAX_REQ_ATTEMPTS indicate no response from server.
	if(number_of_attempts >= MAX_REQ_ATTEMPTS)
		fprintf(stdout, "No Response from Server after %d attempts. Hence closing the connection.\n", MAX_REQ_ATTEMPTS);
	
	return 0;
}