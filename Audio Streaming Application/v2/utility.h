#ifndef UTILITY_H
#define UTILITY_H

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

typedef struct server_configs{
    char *ip; // server IP is stored as a string
	int port; // server port is stored as an integer.
	unsigned short lambda; // initial sending rate in unit of packets per second.
	char *logfile; // logfile name is stored as a string.
	/*
    unsigned short secret_key; // secret key at the server.
	int blocksize; // blocksize for file I/O.
	int windowsize; // windowsize(W) for the number of packets to transmit.
	int timeout; // time to wait before re-transmitting W packets.
	*/
}srv_config;

typedef struct streaming{
	char filename[8]; // file to be transferred to client.
	int blocksize; // blocksize for file I/O.
	unsigned short pspacing; // sleep time between sending packets to client.
	int client_number; // Represents what is the particular client's number.
	char logfile[15]; // logfile name is stored as a string.
	int start_time; // transmission start time.
}streaming_config;

typedef struct client_configs{
    char *server_ip; // server IP is stored as a string
	int server_port; // server port is stored as an integer.
	char *filename; // file to be transferred to client.
	unsigned short blocksize; // blocksize for file I/O.
	int buffersize; // buffersize at the client.
	int targetbuf; // buffersize aiming to be achieved.
	unsigned short lambda; // initial sending rate in unit of packets per second.
	int method; // congestion control method to be used.
	char *logfile; // logfile name is stored as a string.
	size_t audio_block; // audio block size to be sent to mulawwrite() for streaming.
	unsigned short current_bufsize; // current size of the buffer.
	int producer_pointer; // points to the index from where data has to be written to buffer.
	int consumer_pointer; // points to the index from where data has to be read from buffer.
	int last_message_length; // size of the last response received from the server.
	int start_time; // transmission start time.
	unsigned short pspacing;
}cli_config;


/* A wrapper method for perror.
 * It logs the error message and
 * terminates the process.
 */
void do_error(char *msg) {
	perror(msg);
	exit(1);
}

/* The function fetches the current time using 
 * gettimeofday() and returns it in milliseconds.
 */
int current_time_in_msec() {
    struct timeval ct; 
    gettimeofday(&ct, NULL); 
    int msec = ct.tv_sec*1000 + ct.tv_usec/1000; 
    return msec;
}


/* The signal handler to catch the SIGALARM.
 * It is not required to do anything here but 
 * to just stop the program from terminating.
 */
void alarm_handler_util(int sig){}

/* The function calls `setitimer` for ITIMER_REAL
 * to set a timer for give timeout in milliseconds.
 */
void set_timer(int timeout){
	struct itimerval it_val; 
	it_val.it_value.tv_sec = timeout/1000;
	it_val.it_value.tv_usec = (timeout*1000) % 1000000;
	it_val.it_interval = it_val.it_value;
	setitimer(ITIMER_REAL, &it_val, NULL);
}

#endif
